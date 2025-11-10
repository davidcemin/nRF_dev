# WiFi RTP Receiver for nRF7002-DK

A C++ application that receives RTP audio streams over WiFi with **interactive shell control**.

## Overview

Features:
- ✅ **Interactive Shell/CLI** - Manual WiFi connection and RTP control
- ✅ WiFi connection (WPA2) via command line or auto-connect
- ✅ UDP socket server with manual start/stop
- ✅ RTP packet reception and parsing
- ✅ Modern C++ practices (references, std::string, std::array)

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
│   ├── rtp_receiver.cpp
│   ├── shell_commands.hpp # CLI interface
│   └── shell_commands.cpp
└── tools/
    └── stream_audio.py    # Python audio streaming tool
```

## C++ Design Decisions

**Why pointers for output parameters?**
- Output parameters use pointers (e.g., `const uint8_t** payload`) - this is idiomatic for "out" params
- Input parameters use const references (e.g., `const std::string& ssid`) - cleaner than pointers

**Why C arrays instead of std::vector?**
- Fixed-size buffers use stack allocation (C arrays or `std::array<>`) 
- Avoids heap fragmentation in embedded systems
- `std::vector` would require dynamic allocation which is expensive for real-time audio

**Modern C++ where it helps:**
- `std::string` for dynamic text (worth the allocation for flexibility)
- `std::array<>` for fixed-size containers with bounds checking
- References instead of pointers for non-null parameters

## Building the Firmware

### Prerequisites

1. Install [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nrf-connect-sdk) (v2.5.0 or later)
2. Set up Zephyr environment

### Configure WiFi Credentials (Optional)

For **auto-connect mode**, edit `prj.conf`:

```bash
west build -t menuconfig
```

Navigate to: `WiFi RTP Receiver Settings` and set:
- **WIFI_SSID**: Your WiFi network name
- **WIFI_PSK**: Your WiFi password
- **RTP_UDP_PORT**: UDP port (default: 5004)

**Note:** If you leave SSID as "MyNetwork", the device will wait for manual shell commands instead of auto-connecting.

### Build

```bash
# From the audioBle directory
west build -b nrf7002dk

# Flash to device
west flash
```

## Running the Demo

### Option 1: Interactive Shell Mode (Recommended)

### 1. Flash and Monitor the nRF7002-DK

```bash
west flash
west build -t monitor
```

You'll see the shell prompt:
```
*** Booting nRF Connect SDK v2.x.x ***
[00:00:00] <inf> main: === WiFi RTP Receiver ===
[00:00:00] <inf> main: Board: nRF7002-DK
[00:00:00] <inf> main: 
[00:00:00] <inf> main: Use shell commands to control:
[00:00:00] <inf> main:   wifi connect <ssid> <password>  - Connect to WiFi
[00:00:00] <inf> main:   wifi status                     - Show WiFi status
[00:00:00] <inf> main:   rtp start [port]                - Start RTP receiver
[00:00:00] <inf> main:   rtp status                      - Show RTP status
[00:00:00] <inf> main: 
[00:00:00] <inf> main: Ready! Type 'help' for available commands

wifi-rtp:~$ 
```

### 2. Connect to WiFi via Shell

```bash
wifi-rtp:~$ wifi connect MyHomeNetwork MyPassword123
```

Output:
```
Connecting to WiFi: MyHomeNetwork
WiFi connected!
IP Address: 192.168.1.100
```

### 3. Start RTP Receiver

```bash
wifi-rtp:~$ rtp start
```

Output:
```
Starting RTP receiver on port 5004...
RTP receiver started!
Listening on: 192.168.1.100:5004

Stream audio with:
  python3 stream_audio.py song.mp3 192.168.1.100 5004
```

### 4. Stream Audio from Your Mac

See instructions below in "Streaming Audio" section.

### 5. Check Status Anytime

```bash
wifi-rtp:~$ wifi status
WiFi Status: Connected
IP Address: 192.168.1.100

wifi-rtp:~$ rtp status
RTP Receiver: Running
Port: 5004
Address: 192.168.1.100:5004
```

See [SHELL_COMMANDS.md](SHELL_COMMANDS.md) for complete command reference.

---

### Option 2: Auto-Connect Mode

If you configure WiFi credentials in `prj.conf` (change from "MyNetwork"), the device will automatically:
1. Connect to WiFi on boot
2. Start RTP receiver on the configured port

### 1. Flash and Monitor

```bash
west flash
# In another terminal:
west build -t monitor
```

You should see output like:
```
*** Booting nRF Connect SDK v2.x.x ***
[00:00:00] <inf> main: === WiFi RTP Receiver ===
[00:00:00] <inf> main: Auto-connecting to configured WiFi...
[00:00:03] <inf> wifi_manager: WiFi connected successfully
[00:00:05] <inf> wifi_manager: IPv4 address obtained: 192.168.1.100
[00:00:05] <inf> main: WiFi connected: 192.168.1.100
[00:00:05] <inf> main: Starting RTP receiver on port 5004...
[00:00:05] <inf> rtp_receiver: RTP receiver started on port 5004
[00:00:05] <inf> main: RTP receiver started!
[00:00:05] <inf> main: Send audio to: 192.168.1.100:5004
```

**Note the IP address!** You'll need it for streaming.

## Streaming Audio from Mac/PC

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

Replace `192.168.1.100` with your nRF7002-DK's IP address (from `wifi status` or boot logs).

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
