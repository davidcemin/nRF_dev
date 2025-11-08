#include "wifi_manager.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/socket.h>
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
            const struct in_addr *addr = &iface->config.ip.ipv4->unicast[0].address.in_addr;
            inet_ntop(AF_INET, addr, addr_str, sizeof(addr_str));
            LOG_INF("IPv4 address obtained: %s", addr_str);
            k_sem_give(&ipv4_obtained_sem);
        }
    }
}

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

int WiFiManager::connect()
{
    m_iface = net_if_get_default();
    if (!m_iface) {
        LOG_ERR("No default network interface found");
        return -ENODEV;
    }

    // Setup event callbacks
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);

    // Configure WiFi parameters
    struct wifi_connect_req_params params = {0};
    params.ssid = CONFIG_WIFI_SSID;
    params.ssid_length = strlen(CONFIG_WIFI_SSID);
    params.psk = CONFIG_WIFI_PSK;
    params.psk_length = strlen(CONFIG_WIFI_PSK);
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

bool WiFiManager::isConnected() const
{
    return m_connected;
}

int WiFiManager::getIpAddress(char* buffer, size_t size) const
{
    if (!m_iface || !m_iface->config.ip.ipv4) {
        return -EINVAL;
    }

    const struct in_addr *addr = &m_iface->config.ip.ipv4->unicast[0].address.in_addr;
    inet_ntop(AF_INET, addr, buffer, size);
    return 0;
}
