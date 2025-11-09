#include "wifi_manager.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
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
    m_ssid = ssid;
    m_password = password;
    
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
        return -ENODEV;
    }

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

    // Request connection
    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, m_iface, &params,
                       sizeof(struct wifi_connect_req_params));
    if (ret) {
        LOG_ERR("WiFi connection request failed: %d", ret);
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
