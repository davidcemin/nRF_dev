# Project File Structure

```
audioBle/
│
├── CMakeLists.txt           # Build configuration
├── prj.conf                 # Zephyr project config (WiFi, networking, shell)
├── Kconfig                  # Configuration menu definitions
│
├── README.md                # Main documentation
├── QUICKSTART.md            # Quick reference for common tasks
├── SHELL_COMMANDS.md        # Shell command reference
├── DESIGN.md                # C++ design decisions explained
│
├── src/
│   ├── main.cpp             # Application entry point, shell init
│   │
│   ├── wifi_manager.hpp     # WiFi connection management
│   ├── wifi_manager.cpp     # - connect(), disconnect(), getIpAddress()
│   │
│   ├── rtp_receiver.hpp     # RTP/UDP packet reception
│   ├── rtp_receiver.cpp     # - start(), stop(), parseRtpPacket()
│   │
│   ├── shell_commands.hpp   # Shell/CLI interface
│   └── shell_commands.cpp   # - wifi connect/status, rtp start/stop
│
└── tools/
    └── stream_audio.py      # Python RTP audio streamer for Mac/PC

Total: 15 files
- 8 C++ source files (hpp + cpp)
- 4 documentation files (md)
- 3 config files (cmake, conf, kconfig)
- 1 Python tool
```

## Key Files

### For Building
- `CMakeLists.txt` - Build system
- `prj.conf` - Enable features (WiFi, networking, shell, C++)

### For Development
- `src/main.cpp` - Start here
- `src/wifi_manager.*` - WiFi API
- `src/rtp_receiver.*` - RTP API
- `src/shell_commands.*` - CLI commands

### For Users
- `QUICKSTART.md` - Copy-paste commands to get started
- `README.md` - Full documentation
- `tools/stream_audio.py` - Stream audio from Mac

### For Learning
- `DESIGN.md` - Why we made these C++ choices
- `SHELL_COMMANDS.md` - All available commands

## Lines of Code

| Component | Lines | Purpose |
|-----------|-------|---------|
| wifi_manager | ~150 | WiFi connection, event handling |
| rtp_receiver | ~200 | UDP server, RTP parsing |
| shell_commands | ~200 | CLI interface |
| main.cpp | ~50 | Initialization, auto-connect |
| **Total C++** | **~600** | Core application |
| stream_audio.py | ~250 | Mac streaming tool |
| **Total** | **~850** | Complete project |

Clean, focused, maintainable! 🎯
