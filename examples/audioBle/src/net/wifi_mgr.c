#include "wifi_mgr.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(wifi_mgr, LOG_LEVEL_INF);

static K_SEM_DEFINE(wifi_connected, 0, 1);
static K_SEM_DEFINE(ipv4_addr, 0, 1);
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static bool is_connected = false;

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb,
                                       uint32_t mgmt_event, struct net_if *iface)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        if (status->status == 0) {
            LOG_INF("WiFi Connected!");
            is_connected = true;
            k_sem_give(&wifi_connected);
        } else {
            LOG_ERR("WiFi Connection failed: %d", status->status);
            is_connected = false;
        }
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        LOG_INF("WiFi Disconnected");
        is_connected = false;
        k_sem_reset(&wifi_connected);
        k_sem_reset(&ipv4_addr);
    }
}

static void handle_ipv4_result(struct net_mgmt_event_callback *cb,
                               uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        char buf[NET_IPV4_ADDR_LEN];
        
        for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
            struct net_if_addr *if_addr = (struct net_if_addr*)&iface->config.ip.ipv4->unicast[i];
            
            if (if_addr->addr_type == NET_ADDR_DHCP && if_addr->is_used) {
                net_addr_ntop(AF_INET, &if_addr->address.in_addr, buf, sizeof(buf));
                LOG_INF("IPv4 address: %s", buf);
                k_sem_give(&ipv4_addr);
                break;
            }
        }
    }
}

int wifi_mgr_connect(const char *ssid, const char *password)
{
    struct net_if *iface;
    struct wifi_connect_req_params params = {0};
    static bool callbacks_init = false;
    int ret;

    LOG_INF("Connecting to WiFi SSID: %s", ssid);

    /* Get WiFi interface */
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("Default network interface not found");
        return -ENODEV;
    }

    /* Setup callbacks (once) */
    if (!callbacks_init) {
        net_mgmt_init_event_callback(&wifi_cb, handle_wifi_connect_result,
                                     NET_EVENT_WIFI_CONNECT_RESULT |
                                     NET_EVENT_WIFI_DISCONNECT_RESULT);
        net_mgmt_add_event_callback(&wifi_cb);

        net_mgmt_init_event_callback(&ipv4_cb, handle_ipv4_result,
                                     NET_EVENT_IPV4_ADDR_ADD);
        net_mgmt_add_event_callback(&ipv4_cb);
        callbacks_init = true;
        LOG_INF("WiFi callbacks initialized");
    }

    /* Setup connection parameters */
    params.ssid = (uint8_t *)ssid;
    params.ssid_length = strlen(ssid);
    params.psk = (uint8_t *)password;
    params.psk_length = strlen(password);
    params.channel = WIFI_CHANNEL_ANY;
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.band = WIFI_FREQ_BAND_UNKNOWN;
    params.mfp = WIFI_MFP_OPTIONAL;
    params.timeout = SYS_FOREVER_MS;

    /* Request connection */
    LOG_INF("Sending connection request...");
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) {
        LOG_ERR("Connection request failed: %d", ret);
        return ret;
    }

    LOG_INF("Connection request sent, waiting for result...");

    /* Wait for connection */
    if (k_sem_take(&wifi_connected, K_SECONDS(30)) != 0) {
        LOG_ERR("Connection timeout");
        return -ETIMEDOUT;
    }

    LOG_INF("Connected, waiting for IP...");

    /* Wait for IP address */
    if (k_sem_take(&ipv4_addr, K_SECONDS(30)) != 0) {
        LOG_ERR("DHCP timeout");
        return -ETIMEDOUT;
    }

    LOG_INF("WiFi fully connected");
    return 0;
}

void wifi_mgr_disconnect(void)
{
    struct net_if *iface = net_if_get_default();
    
    if (iface) {
        net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
    }
    is_connected = false;
}

bool wifi_mgr_is_connected(void)
{
    return is_connected;
}

int wifi_mgr_get_ip(char *buf, size_t buflen)
{
    struct net_if *iface = net_if_get_default();
    
    if (!iface || !is_connected) {
        return -ENOTCONN;
    }

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        struct net_if_addr *if_addr = (struct net_if_addr*)&iface->config.ip.ipv4->unicast[i];
        
        if (if_addr->addr_type == NET_ADDR_DHCP && if_addr->is_used) {
            return net_addr_ntop(AF_INET, &if_addr->address.in_addr, buf, buflen) ? 0 : -EINVAL;
        }
    }
    
    return -ENOENT;
}
