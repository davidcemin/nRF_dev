#pragma once

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connect to WiFi network
 * @param ssid Network SSID (null-terminated string)
 * @param password Network password (null-terminated string)
 * @return 0 on success, negative error code on failure
 */
int wifi_mgr_connect(const char *ssid, const char *password);

/**
 * @brief Disconnect from WiFi
 */
void wifi_mgr_disconnect(void);

/**
 * @brief Check if connected
 * @return true if connected, false otherwise
 */
bool wifi_mgr_is_connected(void);

/**
 * @brief Get IP address
 * @param buf Buffer to store IP address string
 * @param buflen Buffer length
 * @return 0 on success, negative on error
 */
int wifi_mgr_get_ip(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif
