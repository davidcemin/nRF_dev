#include "shell_commands.hpp"
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(shell_cmds, LOG_LEVEL_INF);

// Global references to managers (set during init)
static WiFiManager* g_wifi = nullptr;
static RtpReceiver* g_rtp = nullptr;

// WiFi connect command
static int cmd_wifi_connect(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_error(sh, "Usage: wifi connect <ssid> <password>");
        return -EINVAL;
    }

    if (!g_wifi) {
        shell_error(sh, "WiFi manager not initialized");
        return -ENODEV;
    }

    const std::string ssid = argv[1];
    const std::string password = argv[2];

    shell_print(sh, "Connecting to WiFi: %s", ssid.c_str());
    
    int ret = g_wifi->connect(ssid, password);
    if (ret < 0) {
        shell_error(sh, "WiFi connection failed: %d", ret);
        return ret;
    }

    shell_print(sh, "WiFi connected!");
    shell_print(sh, "IP Address: %s", g_wifi->getIpAddress().c_str());
    
    return 0;
}

// WiFi disconnect command
static int cmd_wifi_disconnect(const struct shell *sh, size_t argc, char **argv)
{
    if (!g_wifi) {
        shell_error(sh, "WiFi manager not initialized");
        return -ENODEV;
    }

    g_wifi->disconnect();
    shell_print(sh, "WiFi disconnected");
    
    return 0;
}

// WiFi status command
static int cmd_wifi_status(const struct shell *sh, size_t argc, char **argv)
{
    if (!g_wifi) {
        shell_error(sh, "WiFi manager not initialized");
        return -ENODEV;
    }

    if (g_wifi->isConnected()) {
        shell_print(sh, "WiFi Status: Connected");
        shell_print(sh, "IP Address: %s", g_wifi->getIpAddress().c_str());
    } else {
        shell_print(sh, "WiFi Status: Disconnected");
    }
    
    return 0;
}

// RTP start command
static int cmd_rtp_start(const struct shell *sh, size_t argc, char **argv)
{
    if (!g_rtp) {
        shell_error(sh, "RTP receiver not initialized");
        return -ENODEV;
    }

    if (!g_wifi || !g_wifi->isConnected()) {
        shell_error(sh, "WiFi must be connected first");
        return -ENOTCONN;
    }

    uint16_t port = CONFIG_RTP_UDP_PORT;
    
    // Optional port argument
    if (argc > 1) {
        port = static_cast<uint16_t>(atoi(argv[1]));
    }

    if (g_rtp->isRunning()) {
        shell_warn(sh, "RTP receiver already running on port %u", g_rtp->getPort());
        return 0;
    }

    shell_print(sh, "Starting RTP receiver on port %u...", port);
    
    int ret = g_rtp->start(port);
    if (ret < 0) {
        shell_error(sh, "Failed to start RTP receiver: %d", ret);
        return ret;
    }

    shell_print(sh, "RTP receiver started!");
    shell_print(sh, "Listening on: %s:%u", g_wifi->getIpAddress().c_str(), port);
    shell_print(sh, "");
    shell_print(sh, "Stream audio with:");
    shell_print(sh, "  python3 stream_audio.py song.mp3 %s %u", 
                g_wifi->getIpAddress().c_str(), port);
    
    return 0;
}

// RTP stop command
static int cmd_rtp_stop(const struct shell *sh, size_t argc, char **argv)
{
    if (!g_rtp) {
        shell_error(sh, "RTP receiver not initialized");
        return -ENODEV;
    }

    if (!g_rtp->isRunning()) {
        shell_warn(sh, "RTP receiver is not running");
        return 0;
    }

    shell_print(sh, "Stopping RTP receiver...");
    g_rtp->stop();
    shell_print(sh, "RTP receiver stopped");
    
    return 0;
}

// RTP status command
static int cmd_rtp_status(const struct shell *sh, size_t argc, char **argv)
{
    if (!g_rtp) {
        shell_error(sh, "RTP receiver not initialized");
        return -ENODEV;
    }

    if (g_rtp->isRunning()) {
        shell_print(sh, "RTP Receiver: Running");
        shell_print(sh, "Port: %u", g_rtp->getPort());
        if (g_wifi && g_wifi->isConnected()) {
            shell_print(sh, "Address: %s:%u", 
                       g_wifi->getIpAddress().c_str(), g_rtp->getPort());
        }
    } else {
        shell_print(sh, "RTP Receiver: Stopped");
    }
    
    return 0;
}

// Debug/test command
static int cmd_test_print(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "=== Test Build Info ===");
    shell_print(sh, "Build: %s %s", __DATE__, __TIME__);
    
#ifdef CONFIG_WIFI_NRF70
    shell_print(sh, "CONFIG_WIFI_NRF70: ENABLED");
#else
    shell_print(sh, "CONFIG_WIFI_NRF70: DISABLED");
#endif

    shell_print(sh, "Test complete");
    return 0;
}

// Define WiFi subcommands
SHELL_STATIC_SUBCMD_SET_CREATE(wifi_cmds,
    SHELL_CMD_ARG(connect, NULL, 
                  "Connect to WiFi network\n"
                  "Usage: wifi connect <ssid> <password>", 
                  cmd_wifi_connect, 3, 0),
    SHELL_CMD_ARG(disconnect, NULL, 
                  "Disconnect from WiFi", 
                  cmd_wifi_disconnect, 1, 0),
    SHELL_CMD_ARG(status, NULL, 
                  "Show WiFi connection status", 
                  cmd_wifi_status, 1, 0),
    SHELL_SUBCMD_SET_END
);

// Define RTP subcommands
SHELL_STATIC_SUBCMD_SET_CREATE(rtp_cmds,
    SHELL_CMD_ARG(start, NULL, 
                  "Start RTP receiver\n"
                  "Usage: rtp start [port]\n"
                  "  port - UDP port (default: 5004)", 
                  cmd_rtp_start, 1, 1),
    SHELL_CMD_ARG(stop, NULL, 
                  "Stop RTP receiver", 
                  cmd_rtp_stop, 1, 0),
    SHELL_CMD_ARG(status, NULL, 
                  "Show RTP receiver status", 
                  cmd_rtp_status, 1, 0),
    SHELL_SUBCMD_SET_END
);

// Register root commands
SHELL_CMD_REGISTER(wifi, &wifi_cmds, "WiFi management commands", NULL);
SHELL_CMD_REGISTER(rtp, &rtp_cmds, "RTP receiver commands", NULL);
SHELL_CMD_REGISTER(test, NULL, "Test print output", cmd_test_print);

void shell_init(WiFiManager& wifi, RtpReceiver& rtp)
{
    g_wifi = &wifi;
    g_rtp = &rtp;
    LOG_INF("Shell commands initialized");
}
