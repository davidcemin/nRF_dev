#include "wifi_manager.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <errno.h>
#include <cstring>

LOG_MODULE_REGISTER(wifi_manager, LOG_LEVEL_DBG);

static K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static K_SEM_DEFINE(ipv4_obtained_sem, 0, 1);

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                                uint32_t event, struct net_if *iface)
{
    switch (event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        LOG_INF("WiFi connected successfully");
        k_sem_give(&wifi_connected_sem);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_WRN("WiFi disconnected");
        k_sem_reset(&wifi_connected_sem);
        k_sem_reset(&ipv4_obtained_sem);
        break;
    default:
        break;
    }
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb,
                                uint32_t event, struct net_if *iface)
{
    if (event == NET_EVENT_IPV4_ADDR_ADD) {
        char addr_str[NET_IPV4_ADDR_LEN];
        
        if (iface->config.ip.ipv4) {
            const struct in_addr *addr = (const struct in_addr*)&iface->config.ip.ipv4->unicast[0].ipv4;
            inet_ntop(AF_INET, addr, addr_str, sizeof(addr_str));
            LOG_INF("IPv4 address obtained: %s", addr_str);
            k_sem_give(&ipv4_obtained_sem);
        }
    }
}

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

int WiFiManager::connect(const std::string& ssid, const std::string& password)
{
    LOG_INF("========================================");
    LOG_INF("WiFiManager::connect() called - BUILD %s %s", __DATE__, __TIME__);
    LOG_INF("========================================");
    LOG_INF("SSID: %s", ssid.c_str());
    
    m_ssid = ssid;
    m_password = password;
    
    LOG_INF("Attempting to get default network interface...");
    // Wait for network interface to be available
    m_iface = net_if_get_default();
    int retry = 0;
    while (!m_iface && retry < 10) {
        LOG_WRN("Waiting for network interface... (attempt %d)", retry + 1);
        k_sleep(K_MSEC(500));
        m_iface = net_if_get_default();
        retry++;
    }
    
    if (!m_iface) {
        LOG_ERR("No default network interface found after %d attempts", retry);
        LOG_ERR("This means the WiFi driver did NOT initialize!");
        LOG_ERR("Check if CONFIG_WIFI_NRF70=y in build/.config");
        return -ENODEV;
    }

    LOG_INF("Network interface found: %p", m_iface);
    
    // Bring up the network interface if it's not already up
    if (!net_if_is_up(m_iface)) {
        LOG_INF("Bringing up network interface...");
        net_if_up(m_iface);
        k_sleep(K_MSEC(100));
    }

    LOG_INF("Network interface ready");

    // Setup event callbacks (only once)
    static bool callbacks_initialized = false;
    if (!callbacks_initialized) {
        net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                     NET_EVENT_WIFI_CONNECT_RESULT |
                                     NET_EVENT_WIFI_DISCONNECT_RESULT);
        net_mgmt_add_event_callback(&wifi_cb);

        net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler,
                                     NET_EVENT_IPV4_ADDR_ADD);
        net_mgmt_add_event_callback(&ipv4_cb);
        callbacks_initialized = true;
    }

    // Configure WiFi parameters
    struct wifi_connect_req_params params = {0};
    params.ssid = reinterpret_cast<const uint8_t*>(m_ssid.c_str());
    params.ssid_length = m_ssid.length();
    params.psk = reinterpret_cast<const uint8_t*>(m_password.c_str());
    params.psk_length = m_password.length();
    params.channel = WIFI_CHANNEL_ANY;
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.band = WIFI_FREQ_BAND_2_4_GHZ;
    params.mfp = WIFI_MFP_OPTIONAL;

    LOG_INF("Connecting to WiFi SSID: %s", params.ssid);
    LOG_INF("WiFi params: ssid_len=%d, psk_len=%d, security=%d, band=%d, channel=%d",
            params.ssid_length, params.psk_length, params.security, params.band, params.channel);
    LOG_INF("Interface pointer: %p", m_iface);
    LOG_INF("Interface index: %d", net_if_get_by_iface(m_iface));
    
    // Check if interface supports WiFi
    if (!net_if_l2(m_iface)) {
        LOG_ERR("Interface has no L2 layer!");
    } else {
        LOG_INF("Interface L2: %p", net_if_l2(m_iface));
    }
    
    // Get WiFi interface specifically
    struct net_if *wifi_iface = NULL;
    int iface_count = 0;
    
    // Iterate through all interfaces to find WiFi
    for (int i = 1; i <= 10; i++) {
        struct net_if *iface = net_if_get_by_index(i);
        if (!iface) continue;
        
        iface_count++;
        LOG_INF("Found interface %d at %p", i, iface);
        
        // Check if this is a WiFi interface by trying to get capabilities
        struct wifi_iface_status status = {0};
        int ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, 
                          &status, sizeof(status));
        if (ret == 0) {
            LOG_INF("  -> This is a WiFi interface! State: %d", status.state);
            wifi_iface = iface;
            break;
        } else {
            LOG_INF("  -> Not WiFi or not ready (ret=%d)", ret);
        }
    }
    
    if (wifi_iface && wifi_iface != m_iface) {
        LOG_WRN("Default interface is NOT WiFi! Using interface %p instead", wifi_iface);
        m_iface = wifi_iface;
        
        // Bring up the WiFi interface
        if (!net_if_is_up(m_iface)) {
            LOG_INF("Bringing up WiFi interface...");
            net_if_up(m_iface);
            k_sleep(K_MSEC(100));
        }
    } else if (!wifi_iface) {
        LOG_ERR("No WiFi interface found among %d interfaces!", iface_count);
        LOG_ERR("WiFi driver is compiled but no WiFi device registered!");
        return -ENODEV;
    }
    
    // Give the WiFi driver more time to initialize
    LOG_INF("Waiting for WiFi driver to be ready...");
    k_sleep(K_MSEC(2000));
    
    // Trigger a scan first to initialize the nRF7002 firmware download
    LOG_INF("Triggering WiFi scan to initialize nRF7002 chip...");
    int scan_ret = net_mgmt(NET_REQUEST_WIFI_SCAN, m_iface, NULL, 0);
    if (scan_ret == 0) {
        LOG_INF("Scan initiated successfully, waiting for scan to complete...");
        k_sleep(K_MSEC(10000));  // Wait longer for scan to fully complete
    } else {
        LOG_WRN("Scan failed with %d, continuing anyway...", scan_ret);
        k_sleep(K_MSEC(1000));
    }
    
    // Request connection (firmware should now be loaded)
    LOG_INF("Calling net_mgmt(NET_REQUEST_WIFI_CONNECT, ...)");
    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, m_iface, &params,
                       sizeof(struct wifi_connect_req_params));
    if (ret) {
        LOG_ERR("WiFi connection request failed: %d (%s)", ret, 
                ret == -EPERM ? "EPERM" :
                ret == -ENODEV ? "ENODEV" :
                ret == -EINVAL ? "EINVAL" :
                ret == -ENOTSUP ? "ENOTSUP" : "UNKNOWN");
        LOG_ERR("This usually means:");
        LOG_ERR("  -EPERM (-1): WiFi management not available on this interface");
        LOG_ERR("  -ENODEV (-19): No device");
        LOG_ERR("  -EINVAL (-22): Invalid parameters");
        LOG_ERR("  -ENOTSUP (-134): Operation not supported");
        return ret;
    }

    // Wait for connection (30 second timeout)
    if (k_sem_take(&wifi_connected_sem, K_SECONDS(30)) != 0) {
        LOG_ERR("WiFi connection timeout");
        return -ETIMEDOUT;
    }

    // Wait for IPv4 address (30 second timeout)
    if (k_sem_take(&ipv4_obtained_sem, K_SECONDS(30)) != 0) {
        LOG_ERR("IPv4 address timeout");
        return -ETIMEDOUT;
    }

    m_connected = true;
    LOG_INF("WiFi initialization complete");
    return 0;
}

void WiFiManager::disconnect()
{
    if (!m_iface || !m_connected) {
        return;
    }

    int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, m_iface, NULL, 0);
    if (ret) {
        LOG_ERR("WiFi disconnect request failed: %d", ret);
    }
    
    m_connected = false;
}

bool WiFiManager::isConnected() const
{
    return m_connected;
}

std::string WiFiManager::getIpAddress() const
{
    if (!m_iface || !m_iface->config.ip.ipv4) {
        return "";
    }

    std::array<char, NET_IPV4_ADDR_LEN> addr_str;
    const struct in_addr *addr = (const struct in_addr*)&m_iface->config.ip.ipv4->unicast[0].ipv4;
    inet_ntop(AF_INET, addr, addr_str.data(), addr_str.size());
    
    return std::string(addr_str.data());
}
