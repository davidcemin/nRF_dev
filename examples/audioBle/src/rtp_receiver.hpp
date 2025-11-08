#pragma once

#include <zephyr/kernel.h>
#include <cstdint>
#include <cstddef>

// RTP Header structure (RFC 3550)
struct RtpHeader {
    uint8_t vpxcc;      // Version, Padding, Extension, CSRC count
    uint8_t mpt;        // Marker, Payload type
    uint16_t sequence;  // Sequence number
    uint32_t timestamp; // Timestamp
    uint32_t ssrc;      // Synchronization source identifier
};

class RtpReceiver {
public:
    RtpReceiver() = default;
    ~RtpReceiver() = default;

    /**
     * @brief Start UDP server to receive RTP packets
     * @param port UDP port to listen on
     * @return 0 on success, negative error code on failure
     */
    int start(uint16_t port);

    /**
     * @brief Stop the RTP receiver
     */
    void stop();

    /**
     * @brief Check if receiver is running
     */
    bool isRunning() const { return m_running; }

private:
    /**
     * @brief Parse RTP packet and extract payload
     * @param packet Raw packet data
     * @param length Packet length
     * @param payload Output pointer to payload data
     * @param payloadLen Output payload length
     * @return 0 on success, negative error code on failure
     */
    int parseRtpPacket(const uint8_t* packet, size_t length,
                       const uint8_t** payload, size_t* payloadLen);

    /**
     * @brief Background thread function for receiving packets
     */
    static void receiverThread(void* arg1, void* arg2, void* arg3);

    int m_socket = -1;
    bool m_running = false;
    k_tid_t m_thread_id = nullptr;
};
