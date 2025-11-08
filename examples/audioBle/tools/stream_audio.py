#!/usr/bin/env python3
"""
RTP Audio Streamer for macOS
Streams audio file (MP3, WAV, etc.) over RTP/UDP to nRF7002-DK

Requirements:
    pip install pydub

Usage:
    python3 stream_audio.py <audio_file> <target_ip> [port]
    
Example:
    python3 stream_audio.py song.mp3 192.168.1.100 5004
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


class RtpStreamer:
    """Simple RTP audio streamer"""
    
    # RTP payload type for raw PCM audio
    PAYLOAD_TYPE_PCMU = 0  # G.711 μ-law
    PAYLOAD_TYPE_L16 = 11  # Linear PCM 16-bit, 44.1kHz
    
    def __init__(self, target_ip, target_port=5004, sample_rate=48000):
        self.target_ip = target_ip
        self.target_port = target_port
        self.sample_rate = sample_rate
        
        # RTP state
        self.sequence_number = 0
        self.timestamp = 0
        self.ssrc = 0x12345678  # Random source identifier
        
        # Create UDP socket
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
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
        """Send one RTP packet"""
        header = self.create_rtp_header(marker)
        packet = header + audio_data
        
        self.socket.sendto(packet, (self.target_ip, self.target_port))
        
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
        print(f"\nStreaming to {self.target_ip}:{self.target_port}")
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
    if len(sys.argv) < 3:
        print("Usage: python3 stream_audio.py <audio_file> <target_ip> [port]")
        print("\nExample:")
        print("  python3 stream_audio.py music.mp3 192.168.1.100 5004")
        sys.exit(1)
    
    audio_file = sys.argv[1]
    target_ip = sys.argv[2]
    target_port = int(sys.argv[3]) if len(sys.argv) > 3 else 5004
    
    if not Path(audio_file).exists():
        print(f"Error: Audio file not found: {audio_file}")
        sys.exit(1)
    
    streamer = RtpStreamer(target_ip, target_port)
    streamer.stream_audio(audio_file)


if __name__ == "__main__":
    main()
