#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "wifi_manager.hpp"
#include "rtp_receiver.hpp"
#include "shell_commands.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("=== WiFi RTP Receiver ===");
    LOG_INF("Board: nRF7002-DK");
    LOG_INF("");
    LOG_INF("Use shell commands to control:");
    LOG_INF("  wifi connect <ssid> <password>  - Connect to WiFi");
    LOG_INF("  wifi status                     - Show WiFi status");
    LOG_INF("  rtp start [port]                - Start RTP receiver");
    LOG_INF("  rtp status                      - Show RTP status");
    LOG_INF("");

    // Create manager instances
    static WiFiManager wifi;
    static RtpReceiver rtpReceiver;

    // Initialize shell commands
    shell_init(wifi, rtpReceiver);

    LOG_INF("Ready! Type 'help' for available commands");
    LOG_INF("");

    // Auto-connect if credentials are configured
    if (strlen(CONFIG_WIFI_SSID) > 0 && 
        strcmp(CONFIG_WIFI_SSID, "MyNetwork") != 0) {
        
        LOG_INF("Auto-connecting to configured WiFi...");
        int ret = wifi.connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PSK);
        
        if (ret == 0) {
            LOG_INF("WiFi connected: %s", wifi.getIpAddress().c_str());
            LOG_INF("Starting RTP receiver on port %d...", CONFIG_RTP_UDP_PORT);
            
            ret = rtpReceiver.start(CONFIG_RTP_UDP_PORT);
            if (ret == 0) {
                LOG_INF("RTP receiver started!");
                LOG_INF("Send audio to: %s:%d", 
                       wifi.getIpAddress().c_str(), CONFIG_RTP_UDP_PORT);
            }
        }
    }

    // Main loop - shell handles commands
    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
