#pragma once

#include "../net/wifi_manager.hpp"
#include "../net/rtp_receiver.hpp"

/**
 * @brief Initialize shell commands for WiFi and RTP control
 * @param wifi Reference to WiFiManager instance
 * @param rtp Reference to RtpReceiver instance
 */
void shell_init(WiFiManager& wifi, RtpReceiver& rtp);
