#pragma once

#include <zephyr/kernel.h>
#include <cstdint>
#include <cstddef>
#include <array>

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
    ~RtpReceiver();

    /**
     * @brief Connect to RTP server and start receiving packets
     * @param server_ip Server IP address (e.g., "192.168.1.100")
     * @param server_port Server UDP port
     * @return 0 on success, negative error code on failure
     */
    int start(const char* server_ip, uint16_t server_port);

    /**
     * @brief Stop the RTP receiver
     */
    void stop();

    /**
     * @brief Check if receiver is running
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Get server address and port
     */
    const char* getServerIp() const { return m_server_ip; }
    uint16_t getServerPort() const { return m_server_port; }

private:
    /**
     * @brief Parse RTP packet and extract payload
     * @param packet Raw packet data (const reference for input)
     * @param length Packet length
     * @param payload Output pointer to payload data (pointer OK for output param)
     * @param payloadLen Output payload length (pointer OK for output param)
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
    char m_server_ip[16] = {0};  // IPv4 address string
    uint16_t m_server_port = 0;
    k_tid_t m_thread_id = nullptr;
};
