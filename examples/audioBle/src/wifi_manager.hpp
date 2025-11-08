#pragma once

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>

class WiFiManager {
public:
    WiFiManager() = default;
    ~WiFiManager() = default;

    /**
     * @brief Initialize and connect to WiFi network
     * @return 0 on success, negative error code on failure
     */
    int connect();

    /**
     * @brief Check if WiFi is connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * @brief Get the current IP address
     * @param buffer Buffer to store IP address string
     * @param size Size of buffer
     * @return 0 on success, negative error code on failure
     */
    int getIpAddress(char* buffer, size_t size) const;

private:
    bool m_connected = false;
    struct net_if* m_iface = nullptr;
};
