#include "shell_commands.hpp"
#include "../net/wifi_mgr.h"
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/net_if.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(shell_cmds, LOG_LEVEL_INF);

// Global references to managers (set during init)
static RtpReceiver* g_rtp = nullptr;

// Connection status command (workaround for broken wifi status)
static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    shell_print(sh, "=== Connection Status ===");
    
    // Check actual network interface instead of wifi_mgr state
    struct net_if *iface = net_if_get_default();
    if (iface && net_if_is_up(iface)) {
        // Check if we have an IPv4 address
        bool has_ip = false;
        char ip_str[NET_IPV4_ADDR_LEN];
        
        for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
            struct net_if_addr_ipv4 *if_addr = &iface->config.ip.ipv4->unicast[i];
            if (if_addr->ipv4.is_used) {
                net_addr_ntop(AF_INET, &if_addr->ipv4.address.in_addr, ip_str, sizeof(ip_str));
                shell_print(sh, "WiFi: Connected");
                shell_print(sh, "IP Address: %s", ip_str);
                has_ip = true;
                break;
            }
        }
        
        if (!has_ip) {
            shell_print(sh, "WiFi: Interface UP, waiting for IP...");
        }
    } else {
        shell_print(sh, "WiFi: Not connected");
    }
    
    if (g_rtp && g_rtp->isRunning()) {
        shell_print(sh, "RTP: Running on port %u", g_rtp->getPort());
    } else {
        shell_print(sh, "RTP: Stopped");
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

    // Check if WiFi is connected by looking at network interface
    struct net_if *iface = net_if_get_default();
    bool has_ip = false;
    char ip[NET_IPV4_ADDR_LEN] = {0};
    
    if (iface && net_if_is_up(iface)) {
        for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
            struct net_if_addr_ipv4 *if_addr = &iface->config.ip.ipv4->unicast[i];
            if (if_addr->ipv4.is_used) {
                net_addr_ntop(AF_INET, &if_addr->ipv4.address.in_addr, ip, sizeof(ip));
                has_ip = true;
                break;
            }
        }
    }
    
    if (!has_ip) {
        shell_error(sh, "WiFi must be connected first (no IP address)");
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
    shell_print(sh, "Listening on: %s:%u", ip, port);
    shell_print(sh, "");
    shell_print(sh, "Stream audio with:");
    shell_print(sh, "  python3 stream_audio.py song.mp3 %s %u", ip, port);
    
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
        if (wifi_mgr_is_connected()) {
            char ip[16];
            wifi_mgr_get_ip(ip, sizeof(ip));
            shell_print(sh, "Address: %s:%u", ip, g_rtp->getPort());
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

    // Check WiFi connection status
    if (wifi_mgr_is_connected()) {
        shell_print(sh, "WiFi: Connected");
        char ip[16];
        if (wifi_mgr_get_ip(ip, sizeof(ip)) == 0) {
            shell_print(sh, "IP: %s", ip);
        }
    } else {
        shell_print(sh, "WiFi: Not connected");
    }

    shell_print(sh, "Test complete");
    return 0;
}

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

// Register root commands - only RTP and test (WiFi is provided by CONFIG_NET_L2_WIFI_SHELL)
SHELL_CMD_REGISTER(rtp, &rtp_cmds, "RTP receiver commands", NULL);
SHELL_CMD_REGISTER(status, NULL, "Show connection status", cmd_status);
SHELL_CMD_REGISTER(test, NULL, "Test print output", cmd_test_print);

void shell_init(RtpReceiver& rtp)
{
    g_rtp = &rtp;
    LOG_INF("Shell commands initialized");
}
