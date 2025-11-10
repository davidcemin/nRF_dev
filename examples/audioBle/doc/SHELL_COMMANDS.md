# Shell Commands Reference

## WiFi Commands

### Connect to WiFi
```bash
wifi connect <ssid> <password>
```
Example:
```bash
wifi connect MyHomeNetwork MyPassword123
```

### Check WiFi Status
```bash
wifi status
```

### Disconnect from WiFi
```bash
wifi disconnect
```

## RTP Commands

### Start RTP Receiver
```bash
rtp start [port]
```
Examples:
```bash
rtp start           # Uses default port 5004
rtp start 5005      # Uses custom port 5005
```

### Check RTP Status
```bash
rtp status
```

### Stop RTP Receiver
```bash
rtp stop
```

## Example Workflow

1. **Connect to WiFi:**
   ```bash
   wifi connect MyNetwork MyPassword
   ```

2. **Start RTP receiver:**
   ```bash
   rtp start
   ```

3. **Note the IP address and port, then stream from your Mac:**
   ```bash
   python3 stream_audio.py song.mp3 192.168.1.100 5004
   ```

4. **Check status anytime:**
   ```bash
   wifi status
   rtp status
   ```

## Auto-Connect Mode

If you configure WiFi credentials in `prj.conf` (not "MyNetwork"), the device will:
- Automatically connect to WiFi on boot
- Automatically start RTP receiver on default port

You can still use shell commands to manually control everything.
