#pragma once

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <string>
#include <array>

class WiFiManager {
public:
    WiFiManager() = default;
    ~WiFiManager() = default;

    /**
     * @brief Initialize and connect to WiFi network
     * @param ssid Network SSID
     * @param password Network password
     * @return 0 on success, negative error code on failure
     */
    int connect(const std::string& ssid, const std::string& password);

    /**
     * @brief Disconnect from WiFi
     */
    void disconnect();

    /**
     * @brief Check if WiFi is connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * @brief Get the current IP address
     * @return IP address as string, empty if not connected
     */
    std::string getIpAddress() const;

private:
    bool m_connected = false;
    struct net_if* m_iface = nullptr;
    std::string m_ssid;
    std::string m_password;
};
