# Quick Start Guide

## Flash & Connect

```bash
# Build and flash
west build -b nrf7002dk
west flash

# Monitor (in separate terminal)
west build -t monitor
```

## Shell Commands

```bash
# Connect to WiFi
wifi-rtp:~$ wifi connect YourSSID YourPassword

# Start RTP receiver (default port 5004)
wifi-rtp:~$ rtp start

# Check status
wifi-rtp:~$ wifi status
wifi-rtp:~$ rtp status

# Stop receiver
wifi-rtp:~$ rtp stop

# Disconnect WiFi
wifi-rtp:~$ wifi disconnect
```

## Stream from Mac

```bash
# Install dependencies (once)
pip3 install pydub

# Stream audio file
cd tools/
python3 stream_audio.py song.mp3 <nRF_IP_ADDRESS> 5004
```

## Example Session

```bash
wifi-rtp:~$ wifi connect MyNetwork MyPass123
Connecting to WiFi: MyNetwork
WiFi connected!
IP Address: 192.168.1.100

wifi-rtp:~$ rtp start
Starting RTP receiver on port 5004...
RTP receiver started!
Listening on: 192.168.1.100:5004

Stream audio with:
  python3 stream_audio.py song.mp3 192.168.1.100 5004
```

Then on your Mac:
```bash
python3 stream_audio.py ~/Music/test.mp3 192.168.1.100 5004
```

## Troubleshooting

**Can't connect to WiFi?**
- Verify SSID and password
- Check 2.4GHz network (5GHz not supported)
- Try: `wifi status` to see current state

**No RTP packets?**
- Verify both devices on same network
- Check firewall on Mac
- Try: `ping <nrf_ip>`
- Verify port with: `rtp status`

**Need to change port?**
```bash
rtp start 5005  # Use port 5005 instead
```
