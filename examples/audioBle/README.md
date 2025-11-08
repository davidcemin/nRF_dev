# WiFi RTP Receiver for nRF7002-DK

A simple C++ application that receives RTP audio streams over WiFi and can later forward to Bluetooth headphones.

## Overview

This is a **step 1** implementation focusing on:
- ✅ WiFi connection (WPA2)
- ✅ UDP socket server
- ✅ RTP packet reception and parsing
- ✅ Basic audio data extraction

**Coming later:** Bluetooth LE Audio streaming to wireless headphones

## Hardware Required

- **nRF7002-DK** (Nordic Semiconductor development kit)
- WiFi network (2.4GHz)
- Mac/PC for streaming audio

## Architecture

```
┌─────────────┐        WiFi         ┌──────────────┐
│   Mac/PC    │─────────────────────▶│  nRF7002-DK  │
│             │    RTP over UDP      │              │
│ (Python     │       Port 5004      │  (C++ App)   │
│  Streamer)  │                      │              │
└─────────────┘                      └──────────────┘
                                            │
                                            ▼
                                     [Audio Processing]
                                            │
                                            ▼
                                     (Future: BLE Audio)
```

## Project Structure

```
audioBle/
├── CMakeLists.txt          # Build configuration
├── prj.conf                # Zephyr project config
├── Kconfig                 # Configuration menu
├── src/
│   ├── main.cpp           # Main application
│   ├── wifi_manager.hpp   # WiFi connection management
│   ├── wifi_manager.cpp
│   ├── rtp_receiver.hpp   # RTP/UDP packet reception
│   └── rtp_receiver.cpp
└── tools/
    └── stream_audio.py    # Python audio streaming tool
```

## Building the Firmware

### Prerequisites

1. Install [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nrf-connect-sdk) (v2.5.0 or later)
2. Set up Zephyr environment

### Configure WiFi Credentials

Edit `prj.conf` or use menuconfig:

```bash
west build -t menuconfig
```

Navigate to: `WiFi RTP Receiver Settings` and set:
- **WIFI_SSID**: Your WiFi network name
- **WIFI_PSK**: Your WiFi password
- **RTP_UDP_PORT**: UDP port (default: 5004)

Or directly edit the Kconfig values in a `prj.conf` overlay.

### Build

```bash
# From the audioBle directory
west build -b nrf7002dk_nrf5340_cpuapp

# Flash to device
west flash
```

## Running the Demo

### 1. Flash and Monitor the nRF7002-DK

```bash
west flash
# In another terminal:
west build -t monitor
```

You should see output like:
```
*** Booting nRF Connect SDK v2.x.x ***
[00:00:00] <inf> main: === WiFi RTP Receiver Demo ===
[00:00:00] <inf> main: Board: nRF7002-DK
[00:00:00] <inf> wifi_manager: Connecting to WiFi SSID: YourNetwork
[00:00:03] <inf> wifi_manager: WiFi connected successfully
[00:00:05] <inf> wifi_manager: IPv4 address obtained: 192.168.1.100
[00:00:05] <inf> main: ========================================
[00:00:05] <inf> main: WiFi connected!
[00:00:05] <inf> main: IP Address: 192.168.1.100
[00:00:05] <inf> main: Listening on UDP port: 5004
[00:00:05] <inf> main: ========================================
[00:00:05] <inf> rtp_receiver: RTP receiver started on port 5004
```

**Note the IP address!** You'll need it for streaming.

### 2. Stream Audio from Your Mac

Install Python dependencies:
```bash
pip3 install pydub
```

Stream an audio file:
```bash
cd tools/
python3 stream_audio.py /path/to/your/song.mp3 192.168.1.100 5004
```

Replace `192.168.1.100` with your nRF7002-DK's IP address.

### 3. Verify Reception

On the nRF7002-DK console, you should see:
```
[00:01:23] <inf> rtp_receiver: RTP receiver thread started
[00:01:25] <inf> rtp_receiver: Received RTP from 192.168.1.50:54321 - 960 bytes payload
[00:01:25] <dbg> rtp_receiver: RTP: seq=1, ts=0, pt=11, payload=960 bytes
[00:01:25] <inf> rtp_receiver: Received RTP from 192.168.1.50:54321 - 960 bytes payload
...
```

## Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_WIFI_SSID` | "MyNetwork" | WiFi network name |
| `CONFIG_WIFI_PSK` | "MyPassword" | WiFi password |
| `CONFIG_RTP_UDP_PORT` | 5004 | UDP port for RTP |

## Technical Details

### RTP Format
- **Payload Type**: 11 (L16 - Linear PCM 16-bit)
- **Sample Rate**: 48000 Hz
- **Channels**: Mono
- **Packet Size**: ~20ms of audio (960 bytes @ 48kHz)

### WiFi
- **Frequency**: 2.4 GHz
- **Security**: WPA2-PSK
- **DHCP**: Enabled

## Troubleshooting

### WiFi won't connect
- Verify SSID and password in `prj.conf`
- Ensure you're using 2.4GHz (5GHz not supported)
- Check signal strength

### No RTP packets received
- Verify IP address matches
- Check firewall on your Mac
- Ensure both devices are on same network
- Try: `ping <nrf_ip_address>`

### Build errors
- Ensure nRF Connect SDK is properly installed
- Check Zephyr version compatibility
- Try: `west update`

## Next Steps

Future enhancements:
1. ✅ **Current**: WiFi RTP reception
2. 🔲 Audio buffering and processing
3. 🔲 LC3 codec integration
4. 🔲 Bluetooth LE Audio transmission
5. 🔲 Connection to wireless headphones

## License

This project is for educational/development purposes.

## Resources

- [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
- [nRF7002-DK Product Page](https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK)
- [RTP RFC 3550](https://tools.ietf.org/html/rfc3550)
