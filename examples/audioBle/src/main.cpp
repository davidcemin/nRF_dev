#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/net_if.h>
#include "net/wifi_mgr.h"
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
    LOG_INF("  wifi connect -s <ssid> -p <password>  - Connect to WiFi");
    LOG_INF("  wifi scan                             - Scan for networks");
    LOG_INF("  wifi disconnect                       - Disconnect");
    LOG_INF("  rtp start [port]                      - Start RTP receiver");
    LOG_INF("  rtp status                            - Show RTP status");
    LOG_INF("");

    // Initialize network stack - CRITICAL for WiFi to work!
    printk("Initializing network configuration...\n");
#ifdef CONFIG_NET_CONFIG_SETTINGS
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_wifi));
    if (device_is_ready(dev)) {
        struct net_if *wifi_iface = net_if_lookup_by_dev(dev);
        if (wifi_iface) {
            // Set WiFi as default interface
            net_if_set_default(wifi_iface);
            LOG_INF("WiFi interface set as default");
            
            // Initialize network config (starts DHCP, etc.)
            net_config_init_app(dev, "Initializing network");
            LOG_INF("Network configuration initialized");
            
            // Explicitly bring up the WiFi interface
            if (!net_if_is_up(wifi_iface)) {
                LOG_INF("Bringing up WiFi interface...");
                net_if_up(wifi_iface);
                k_sleep(K_SECONDS(1));
                
                if (net_if_is_up(wifi_iface)) {
                    LOG_INF("WiFi interface is now UP");
                } else {
                    LOG_ERR("Failed to bring up WiFi interface!");
                }
            } else {
                LOG_INF("WiFi interface already UP");
            }
        } else {
            LOG_ERR("Failed to get WiFi network interface!");
        }
    } else {
        LOG_ERR("WiFi device not ready!");
    }
#else
    LOG_WRN("CONFIG_NET_CONFIG_SETTINGS not enabled - network may not work properly");
#endif

    printk("About to create RtpReceiver instance...\n");
    static RtpReceiver* rtpReceiver = nullptr;
    rtpReceiver = new RtpReceiver();
    printk("RtpReceiver created successfully at %p\n", rtpReceiver);
    
    LOG_INF("RtpReceiver created successfully");

    // Initialize shell commands
    printk("Initializing shell commands...\n");
    shell_init(*rtpReceiver);
    printk("Shell commands initialized\n");
    LOG_INF("Shell commands initialized");

    printk("Ready! Type 'help' for available commands\n\n");
    LOG_INF("Ready! Type 'help' for available commands");
    LOG_INF("");

    printk("Entering main loop...\n");
    // Main loop - shell handles commands
    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
