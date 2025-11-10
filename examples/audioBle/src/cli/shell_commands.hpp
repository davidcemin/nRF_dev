#pragma once

#include "../net/rtp_receiver.hpp"

/**
 * @brief Initialize shell commands for WiFi and RTP control
 * @param rtp Reference to RtpReceiver instance
 */
void shell_init(RtpReceiver& rtp);
