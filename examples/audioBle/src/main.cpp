#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "wifi_manager.hpp"
#include "rtp_receiver.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("=== WiFi RTP Receiver Demo ===");
    LOG_INF("Board: nRF7002-DK");
    LOG_INF("Listening for RTP audio on UDP port %d", CONFIG_RTP_UDP_PORT);

    // Initialize WiFi
    WiFiManager wifi;
    LOG_INF("Initializing WiFi...");
    
    int ret = wifi.connect();
    if (ret < 0) {
        LOG_ERR("WiFi connection failed: %d", ret);
        return ret;
    }

    // Display IP address
    char ip_addr[16];
    if (wifi.getIpAddress(ip_addr, sizeof(ip_addr)) == 0) {
        LOG_INF("========================================");
        LOG_INF("WiFi connected!");
        LOG_INF("IP Address: %s", ip_addr);
        LOG_INF("Listening on UDP port: %d", CONFIG_RTP_UDP_PORT);
        LOG_INF("========================================");
    }

    // Start RTP receiver
    RtpReceiver rtpReceiver;
    ret = rtpReceiver.start(CONFIG_RTP_UDP_PORT);
    if (ret < 0) {
        LOG_ERR("Failed to start RTP receiver: %d", ret);
        return ret;
    }

    LOG_INF("RTP receiver is running. Waiting for audio packets...");
    LOG_INF("Send RTP audio to %s:%d", ip_addr, CONFIG_RTP_UDP_PORT);

    // Main loop - just keep alive
    while (1) {
        if (!wifi.isConnected()) {
            LOG_WRN("WiFi disconnected!");
            break;
        }
        k_sleep(K_SECONDS(5));
    }

    rtpReceiver.stop();
    LOG_INF("Application stopped");
    return 0;
}
