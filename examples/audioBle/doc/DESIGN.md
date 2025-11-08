# C++ Design Decisions for Embedded

## Why Pointers vs References?

### Input Parameters: Use `const` References
```cpp
// ✅ Good - const reference for input
int connect(const std::string& ssid, const std::string& password);

// ❌ Avoid - pointer for simple input
int connect(const std::string* ssid, const std::string* password);
```
**Reason:** References are cleaner, can't be null, and show intent (read-only input)

### Output Parameters: Use Pointers
```cpp
// ✅ Good - pointer for output parameter
int parseRtpPacket(const uint8_t* packet, size_t length,
                   const uint8_t** payload, size_t* payloadLen);

// ❌ Less clear - reference for output
int parseRtpPacket(const uint8_t* packet, size_t length,
                   const uint8_t*& payload, size_t& payloadLen);
```
**Reason:** Pointers clearly indicate "output parameter" in C/C++ embedded code. This matches Zephyr's API style.

## Why C Arrays vs std::vector?

### Fixed Buffers: Use Stack Allocation
```cpp
// ✅ Good - stack allocated, no heap fragmentation
uint8_t buffer[RTP_BUFFER_SIZE];  // C array
std::array<char, 16> ip_addr;     // C++ array with bounds checking

// ❌ Avoid in real-time paths - heap allocation
std::vector<uint8_t> buffer(RTP_BUFFER_SIZE);
```

**Reasons:**
1. **No heap fragmentation** - Critical in long-running embedded systems
2. **Deterministic performance** - No allocation overhead in real-time audio path
3. **Stack is faster** - Direct memory access, no indirection
4. **Predictable memory** - Known at compile time

### When std::vector IS Acceptable
- One-time initialization (not in real-time loops)
- Non-critical paths (logging, configuration)
- When size truly can't be determined at compile time

## Modern C++ That Helps

### Use std::string for Dynamic Text
```cpp
// ✅ Good - flexibility worth the cost
std::string getIpAddress() const;

// Could use C strings, but less safe
const char* getIpAddress() const;  // Who owns this memory?
```
**Reason:** WiFi SSID/passwords aren't in real-time path. Safety > performance here.

### Use std::array for Fixed-Size Containers
```cpp
// ✅ Good - bounds checking, iterators, modern C++
std::array<char, NET_IPV4_ADDR_LEN> addr_str;

// Also fine - traditional C
char addr_str[NET_IPV4_ADDR_LEN];
```
**Reason:** std::array gives you C++ features (`.size()`, bounds checking in debug) with zero overhead.

### Use Smart Pointers for Owned Resources
```cpp
// For future work with dynamic objects:
std::unique_ptr<AudioBuffer> buffer;  // Clear ownership
```

## What We Avoided and Why

### ❌ std::vector in RTP Receive Path
```cpp
// BAD - allocates on every packet!
void onRtpPacket(std::vector<uint8_t> data) {
    // Processing...
}
```
**Impact:** 50+ allocations/sec at 20ms packet rate = heap fragmentation

### ❌ Dynamic Allocation in Interrupt Context
```cpp
// NEVER do this in ISR/real-time:
auto data = new uint8_t[size];  // Disaster!
```

### ❌ Exceptions in Real-Time Code
```cpp
// Avoid exception-throwing STL operations in audio path
// (We use -fno-exceptions in embedded anyway)
```

## Memory Layout Example

```
Stack (fast, deterministic):
├── RTP receive buffer [2048 bytes]
├── IP address string [16 bytes]  
└── Local variables

Heap (used sparingly):
├── WiFi credentials (std::string) - once at startup
├── Log messages (std::string) - non-critical path
└── [Future: Audio ring buffer] - allocated once

Static/BSS:
├── WiFiManager instance
├── RtpReceiver instance
└── Zephyr kernel objects (semaphores, etc.)
```

## Performance Characteristics

| Feature | Allocation | Real-time Safe? | Overhead |
|---------|-----------|-----------------|----------|
| C array | Stack | ✅ Yes | Zero |
| std::array | Stack | ✅ Yes | Zero |
| std::string | Heap | ⚠️ Startup only | Small |
| std::vector | Heap | ❌ No | Per-operation |
| const& param | None | ✅ Yes | Zero |
| Pointer param | None | ✅ Yes | Zero |

## Summary

**The Golden Rule:** 
- Use modern C++ for safety and clarity
- But respect real-time constraints
- Stack allocation for audio/network buffers
- Heap allocation for one-time configuration

Our code balances:
- ✅ Modern C++ where it helps (string, array, references)
- ✅ Traditional C where needed (fixed buffers, Zephyr APIs)
- ✅ Performance for real-time audio streaming
- ✅ Safety and maintainability
