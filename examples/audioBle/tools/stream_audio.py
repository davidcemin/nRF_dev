#!/usr/bin/env python3
"""
RTP Audio Server for macOS
Acts as RTP server that streams audio to connected nRF7002-DK client

Requirements:
    pip install pydub

Usage:
    python3 stream_audio.py <audio_file> [port]
    
Example:
    python3 stream_audio.py song.mp3 5004
    
Then on nRF7002-DK:
    uart:~$ wifi connect -s <ssid> -p <password> -k 1
    uart:~$ rtp start <this_computer_ip> 5004
"""

import sys
import socket
import time
import struct
from pathlib import Path

try:
    from pydub import AudioSegment
except ImportError:
    print("Error: pydub is not installed")
    print("Install it with: pip3 install pydub")
    sys.exit(1)


class RtpServer:
    """RTP audio server - waits for client connection and streams audio"""
    
    # RTP payload type for raw PCM audio
    PAYLOAD_TYPE_PCMU = 0  # G.711 μ-law
    PAYLOAD_TYPE_L16 = 11  # Linear PCM 16-bit, 44.1kHz
    
    def __init__(self, port=5004, sample_rate=48000):
        self.port = port
        self.sample_rate = sample_rate
        
        # RTP state
        self.sequence_number = 0
        self.timestamp = 0
        self.ssrc = 0x12345678  # Random source identifier
        
        # Create UDP socket and bind to port
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('0.0.0.0', port))
        
        # Client address (will be set when client connects)
        self.client_addr = None
        
        print(f"RTP Server started on port {port}")
        print(f"Waiting for client connection...")
        print(f"Server IPs:")
        self._print_local_ips()
        print("")
    
    def _print_local_ips(self):
        """Print all local IP addresses"""
        import socket as sock
        hostname = sock.gethostname()
        try:
            # Get all IP addresses
            ips = sock.getaddrinfo(hostname, None)
            seen = set()
            for ip in ips:
                addr = ip[4][0]
                # Filter out IPv6 and localhost
                if ':' not in addr and addr != '127.0.0.1' and addr not in seen:
                    print(f"  {addr}")
                    seen.add(addr)
        except:
            print("  Unable to determine IP address")
    
    def wait_for_client(self):
        """Wait for client to send hello packet"""
        print("\nConnect your nRF device with:")
        print("  uart:~$ wifi connect -s <ssid> -p <password> -k 1")
        print(f"  uart:~$ rtp start {self._get_primary_ip()} {self.port}")
        print(f"\nWaiting for client to connect (timeout 60s)...")
        
        self.socket.settimeout(60)
        
        try:
            # Wait for any packet from client
            data, addr = self.socket.recvfrom(1024)
            self.client_addr = addr
            print(f"✓ Client connected from {addr[0]}:{addr[1]}")
            
            # Set socket back to non-blocking for streaming
            self.socket.settimeout(0.1)  # Small timeout for recv
            return True
        except socket.timeout:
            print("✗ Timeout - no client connected")
            return False
    
    def _get_primary_ip(self):
        """Get the primary local IP address"""
        import socket as sock
        try:
            # Create a socket to determine the primary IP
            s = sock.socket(sock.AF_INET, sock.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
            return ip
        except:
            return "192.168.x.x"
        
    def create_rtp_header(self, marker=False):
        """Create RTP header (12 bytes)"""
        # Version (2 bits) = 2
        # Padding (1 bit) = 0
        # Extension (1 bit) = 0
        # CSRC count (4 bits) = 0
        vpxcc = 0x80  # Version 2, no padding, no extension, no CSRC
        
        # Marker (1 bit) + Payload type (7 bits)
        mpt = (0x80 if marker else 0x00) | self.PAYLOAD_TYPE_L16
        
        # Pack header: Big-endian format
        header = struct.pack('!BBHII',
            vpxcc,                      # V, P, X, CC
            mpt,                        # M, PT
            self.sequence_number,       # Sequence number
            self.timestamp,             # Timestamp
            self.ssrc                   # SSRC
        )
        
        return header
    
    def send_rtp_packet(self, audio_data, marker=False):
        """Send one RTP packet to connected client"""
        if not self.client_addr:
            return
            
        header = self.create_rtp_header(marker)
        packet = header + audio_data
        
        self.socket.sendto(packet, self.client_addr)
        
        # Update sequence number (wraps at 65535)
        self.sequence_number = (self.sequence_number + 1) & 0xFFFF
        
    def stream_audio(self, audio_file, chunk_duration_ms=20):
        """
        Stream audio file over RTP
        
        Args:
            audio_file: Path to audio file (MP3, WAV, etc.)
            chunk_duration_ms: Duration of each RTP packet in milliseconds
        """
        print(f"Loading audio file: {audio_file}")
        
        # Load and convert audio
        audio = AudioSegment.from_file(audio_file)
        
        # Convert to target format
        audio = audio.set_frame_rate(self.sample_rate)
        audio = audio.set_channels(1)  # Mono
        audio = audio.set_sample_width(2)  # 16-bit
        
        print(f"Audio format:")
        print(f"  Sample rate: {audio.frame_rate} Hz")
        print(f"  Channels: {audio.channels}")
        print(f"  Sample width: {audio.sample_width * 8} bits")
        print(f"  Duration: {len(audio) / 1000:.2f} seconds")
        
        if not self.wait_for_client():
            print("No client connected. Exiting.")
            return
        
        print(f"\n🎵 Streaming to client: {self.client_addr[0]}:{self.client_addr[1]}")
        print("Press Ctrl+C to stop\n")
        
        # Get raw audio data
        raw_data = audio.raw_data
        
        # Calculate chunk size in bytes
        bytes_per_sample = audio.sample_width * audio.channels
        samples_per_chunk = int((self.sample_rate * chunk_duration_ms) / 1000)
        chunk_size = samples_per_chunk * bytes_per_sample
        
        # Timestamp increment per packet
        timestamp_increment = samples_per_chunk
        
        print(f"Packet size: {chunk_size} bytes ({chunk_duration_ms}ms)")
        print(f"Timestamp increment: {timestamp_increment}")
        print("-" * 50)
        
        # Stream audio in chunks
        packet_count = 0
        start_time = time.time()
        
        try:
            for i in range(0, len(raw_data), chunk_size):
                chunk = raw_data[i:i + chunk_size]
                
                # Pad last chunk if necessary
                if len(chunk) < chunk_size:
                    chunk += b'\x00' * (chunk_size - len(chunk))
                
                # Send RTP packet
                marker = (i == 0)  # Set marker on first packet
                self.send_rtp_packet(chunk, marker)
                
                packet_count += 1
                self.timestamp += timestamp_increment
                
                # Log progress
                if packet_count % 50 == 0:
                    elapsed = time.time() - start_time
                    audio_time = (i / len(raw_data)) * (len(audio) / 1000)
                    print(f"Sent {packet_count} packets | "
                          f"Audio time: {audio_time:.1f}s | "
                          f"Elapsed: {elapsed:.1f}s")
                
                # Sleep to maintain real-time streaming
                # (slightly faster than real-time to account for processing)
                time.sleep(chunk_duration_ms / 1000 * 0.95)
                
        except KeyboardInterrupt:
            print("\n\nStreaming stopped by user")
        
        elapsed = time.time() - start_time
        print(f"\nStreaming complete:")
        print(f"  Total packets sent: {packet_count}")
        print(f"  Total time: {elapsed:.2f} seconds")
        print(f"  Data rate: {(packet_count * chunk_size / elapsed / 1024):.2f} KB/s")


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 stream_audio.py <audio_file> [port]")
        print("\nExample:")
        print("  python3 stream_audio.py music.mp3 5004")
        print("\nThen on nRF7002-DK:")
        print("  uart:~$ wifi connect -s <ssid> -p <password> -k 1")
        print("  uart:~$ rtp start <this_computer_ip> 5004")
        sys.exit(1)
    
    audio_file = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 5004
    
    if not Path(audio_file).exists():
        print(f"Error: Audio file not found: {audio_file}")
        sys.exit(1)
    
    server = RtpServer(port)
    server.stream_audio(audio_file)


if __name__ == "__main__":
    main()
