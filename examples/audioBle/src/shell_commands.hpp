#pragma once

#include "wifi_manager.hpp"
#include "rtp_receiver.hpp"

/**
 * @brief Initialize shell commands for WiFi and RTP control
 * @param wifi Reference to WiFiManager instance
 * @param rtp Reference to RtpReceiver instance
 */
void shell_init(WiFiManager& wifi, RtpReceiver& rtp);
