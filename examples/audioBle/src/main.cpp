#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include "net/wifi_manager.hpp"
#include "net/rtp_receiver.hpp"
#include "cli/shell_commands.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    // Wait a bit for console to be ready
    k_sleep(K_MSEC(100));
    
    // Use printk to bypass logging system for debugging
    printk("\n\n");
    printk("========================================\n");
    printk("=== MAIN() STARTED - BUILD %s %s ===\n", __DATE__, __TIME__);
    printk("========================================\n");
    printk("If you see this, printk is working!\n");
    printk("========================================\n");
    
    LOG_INF("========================================");
    LOG_INF("=== WiFi RTP Receiver - BUILD %s %s ===", __DATE__, __TIME__);
    LOG_INF("========================================");
    LOG_INF("Board: nRF7002-DK");
    LOG_INF("Firmware compiled: %s at %s", __DATE__, __TIME__);
    
    // Check WiFi driver configuration at compile time
#ifdef CONFIG_WIFI_NRF70
    printk("CONFIG_WIFI_NRF70: ENABLED\n");
    LOG_INF("CONFIG_WIFI_NRF70: ENABLED");
#else
    printk("CONFIG_WIFI_NRF70: DISABLED!!!\n");
    LOG_ERR("CONFIG_WIFI_NRF70: DISABLED!!!");
    LOG_ERR("WiFi driver is NOT compiled in!");
#endif

#ifdef CONFIG_WIFI
    printk("CONFIG_WIFI: ENABLED\n");
    LOG_INF("CONFIG_WIFI: ENABLED");
#else
    printk("CONFIG_WIFI: DISABLED!!!\n");
    LOG_ERR("CONFIG_WIFI: DISABLED!!!");
#endif

#ifdef CONFIG_NETWORKING
    printk("CONFIG_NETWORKING: ENABLED\n");
    LOG_INF("CONFIG_NETWORKING: ENABLED");
#else
    printk("CONFIG_NETWORKING: DISABLED!!!\n");
    LOG_ERR("CONFIG_NETWORKING: DISABLED!!!");
#endif
    
    printk("\n");
    LOG_INF("Use shell commands to control:");
    LOG_INF("  wifi connect <ssid> <password>  - Connect to WiFi");
    LOG_INF("  wifi status                     - Show WiFi status");
    LOG_INF("  rtp start [port]                - Start RTP receiver");
    LOG_INF("  rtp status                      - Show RTP status");
    LOG_INF("");

    printk("About to create WiFiManager instance...\n");
    
    // Create manager instances - use heap to avoid stack issues
    printk("Allocating WiFiManager...\n");
    static WiFiManager* wifi = nullptr;
    wifi = new WiFiManager();
    printk("WiFiManager created successfully at %p\n", wifi);
    
    LOG_INF("WiFiManager created successfully");
    
    printk("About to create RtpReceiver instance...\n");
    static RtpReceiver* rtpReceiver = nullptr;
    rtpReceiver = new RtpReceiver();
    printk("RtpReceiver created successfully at %p\n", rtpReceiver);
    
    LOG_INF("RtpReceiver created successfully");

    // Initialize shell commands
    printk("Initializing shell commands...\n");
    shell_init(*wifi, *rtpReceiver);
    printk("Shell commands initialized\n");
    LOG_INF("Shell commands initialized");

    printk("Ready! Type 'help' for available commands\n\n");
    LOG_INF("Ready! Type 'help' for available commands");
    LOG_INF("");

    // Auto-connect if credentials are configured
    if (strlen(CONFIG_WIFI_SSID) > 0 && 
        strcmp(CONFIG_WIFI_SSID, "MyNetwork") != 0) {
        
        LOG_INF("Auto-connecting to configured WiFi...");
        int ret = wifi->connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PSK);
        
        if (ret == 0) {
            LOG_INF("WiFi connected: %s", wifi->getIpAddress().c_str());
            LOG_INF("Starting RTP receiver on port %d...", CONFIG_RTP_UDP_PORT);
            
            ret = rtpReceiver->start(CONFIG_RTP_UDP_PORT);
            if (ret == 0) {
                LOG_INF("RTP receiver started!");
                LOG_INF("Send audio to: %s:%d", 
                       wifi->getIpAddress().c_str(), CONFIG_RTP_UDP_PORT);
            }
        }
    }

    printk("Entering main loop...\n");
    // Main loop - shell handles commands
    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
