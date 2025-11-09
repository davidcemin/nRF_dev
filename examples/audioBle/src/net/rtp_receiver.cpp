#include "rtp_receiver.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/fcntl.h>

LOG_MODULE_REGISTER(rtp_receiver, LOG_LEVEL_DBG);

#define RTP_BUFFER_SIZE 2048
#define RTP_THREAD_STACK_SIZE 4096
#define RTP_THREAD_PRIORITY 7

static K_THREAD_STACK_DEFINE(rtp_thread_stack, RTP_THREAD_STACK_SIZE);
static struct k_thread rtp_thread_data;

int RtpReceiver::parseRtpPacket(const uint8_t* packet, size_t length,
                                 const uint8_t** payload, size_t* payloadLen)
{
    static uint32_t packet_count = 0;
    
    if (length < sizeof(RtpHeader)) {
        LOG_WRN("Packet too small for RTP header");
        return -EINVAL;
    }

    const RtpHeader* header = reinterpret_cast<const RtpHeader*>(packet);
    
    // Extract version (top 2 bits)
    uint8_t version = (header->vpxcc >> 6) & 0x03;
    if (version != 2) {
        LOG_WRN("Invalid RTP version: %u", version);
        return -EINVAL;
    }

    // Extract fields
    uint8_t padding = (header->vpxcc >> 5) & 0x01;
    uint8_t extension = (header->vpxcc >> 4) & 0x01;
    uint8_t csrcCount = header->vpxcc & 0x0F;
    uint8_t marker = (header->mpt >> 7) & 0x01;
    uint8_t payloadType = header->mpt & 0x7F;
    uint16_t sequence = ntohs(header->sequence);
    uint32_t timestamp = ntohl(header->timestamp);
    uint32_t ssrc = ntohl(header->ssrc);

    // Calculate header size
    size_t headerSize = sizeof(RtpHeader) + (csrcCount * 4);
    
    // Handle extension if present
    if (extension) {
        if (length < headerSize + 4) {
            return -EINVAL;
        }
        uint16_t extLen = ntohs(*reinterpret_cast<const uint16_t*>(packet + headerSize + 2));
        headerSize += 4 + (extLen * 4);
    }

    if (headerSize > length) {
        LOG_WRN("Invalid RTP header size");
        return -EINVAL;
    }

    // Extract payload
    *payload = packet + headerSize;
    *payloadLen = length - headerSize;

    // Handle padding
    if (padding && *payloadLen > 0) {
        uint8_t paddingLen = (*payload)[*payloadLen - 1];
        if (paddingLen <= *payloadLen) {
            *payloadLen -= paddingLen;
        }
    }

    // Log detailed info for first few packets
    packet_count++;
    if (packet_count <= 3) {
        LOG_INF("RTP #%u: seq=%u, ts=%u, pt=%u, ssrc=0x%08x, marker=%u, payload=%u bytes", 
                packet_count, sequence, timestamp, payloadType, ssrc, marker, *payloadLen);
    } else {
        LOG_DBG("RTP: seq=%u, ts=%u, pt=%u, payload=%u bytes", 
                sequence, timestamp, payloadType, *payloadLen);
    }

    return 0;
}

void RtpReceiver::receiverThread(void* arg1, void* arg2, void* arg3)
{
    RtpReceiver* receiver = static_cast<RtpReceiver*>(arg1);
    uint8_t buffer[RTP_BUFFER_SIZE];
    uint32_t packet_count = 0;
    uint32_t bytes_received = 0;
    uint32_t last_report_time = k_uptime_get_32();
    
    LOG_INF("RTP receiver thread started, connected to %s:%u", 
            receiver->m_server_ip, receiver->m_server_port);

    while (receiver->m_running) {
        ssize_t len = recv(receiver->m_socket, buffer, sizeof(buffer), 0);

        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                k_sleep(K_MSEC(10));
                continue;
            }
            LOG_ERR("recv error: %d", errno);
            k_sleep(K_MSEC(100));
            continue;
        }

        if (len > 0) {
            const uint8_t* payload;
            size_t payloadLen;
            
            if (receiver->parseRtpPacket(buffer, len, &payload, &payloadLen) == 0) {
                packet_count++;
                bytes_received += payloadLen;
                
                // Log first packet
                if (packet_count == 1) {
                    LOG_INF("*** First RTP packet received! Payload: %u bytes", payloadLen);
                }
                
                // Report statistics every 5 seconds
                uint32_t now = k_uptime_get_32();
                if (now - last_report_time >= 5000) {
                    uint32_t elapsed_sec = (now - last_report_time) / 1000;
                    uint32_t kbps = (bytes_received * 8) / (elapsed_sec * 1000);
                    LOG_INF("=== RTP Stats: %u packets | %u KB received | %u kbps ===", 
                            packet_count, bytes_received / 1024, kbps);
                    last_report_time = now;
                    bytes_received = 0;  // Reset for next interval
                }
                
                // Here you would forward the payload to audio processing
                // For now, just count and log statistics
            }
        }
    }

    LOG_INF("RTP receiver thread stopped - Total packets: %u", packet_count);
}

RtpReceiver::~RtpReceiver()
{
    stop();
}

int RtpReceiver::start(const char* server_ip, uint16_t server_port)
{
    if (m_running) {
        LOG_WRN("RTP receiver already running");
        return -EALREADY;
    }

    if (!server_ip || server_port == 0) {
        LOG_ERR("Invalid server address or port");
        return -EINVAL;
    }

    // Store server info
    strncpy(m_server_ip, server_ip, sizeof(m_server_ip) - 1);
    m_server_port = server_port;

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        return -errno;
    }

    // Set socket to non-blocking
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

    // Connect to server (for UDP, this filters packets to only this source)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        LOG_ERR("Invalid IP address: %s", server_ip);
        close(m_socket);
        m_socket = -1;
        return -EINVAL;
    }

    if (connect(m_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERR("Failed to connect to %s:%u: %d", server_ip, server_port, errno);
        close(m_socket);
        m_socket = -1;
        return -errno;
    }

    LOG_INF("Connected to RTP server %s:%u", server_ip, server_port);

    // Send a hello packet to let server know we're ready
    // (UDP connect() doesn't actually send anything)
    const char* hello = "RTP_CLIENT_READY";
    ssize_t sent = send(m_socket, hello, strlen(hello), 0);
    if (sent < 0) {
        LOG_WRN("Failed to send hello packet: %d", errno);
    } else {
        LOG_DBG("Sent hello packet to server (%d bytes)", sent);
    }

    // Start receiver thread
    m_running = true;
    m_thread_id = k_thread_create(&rtp_thread_data, rtp_thread_stack,
                                   K_THREAD_STACK_SIZEOF(rtp_thread_stack),
                                   receiverThread,
                                   this, NULL, NULL,
                                   RTP_THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_name_set(m_thread_id, "rtp_receiver");

    LOG_INF("RTP receiver started");
    return 0;
}

void RtpReceiver::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    // Wait for thread to finish
    if (m_thread_id) {
        k_thread_join(m_thread_id, K_FOREVER);
        m_thread_id = nullptr;
    }

    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }

    LOG_INF("RTP receiver stopped");
}
