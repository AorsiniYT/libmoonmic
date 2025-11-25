# libmoonmic

Cross-platform microphone capture library for Moonlight clients with UDP transmission to host.

## Overview

**libmoonmic** is a modular C library that enables microphone audio capture and transmission from Moonlight clients to a host application. It provides:

- **Client Library**: Captures microphone audio and transmits via UDP with Opus encoding
- **Host Application**: Receives audio and injects into virtual audio device
- **Platform Support**: PS Vita, Windows, Linux (macOS and Android ready for extension)

## Architecture

```
┌─────────────────┐         UDP          ┌──────────────────┐
│  libmoonmic     │ ───────────────────> │  moonmic-host    │
│  (Client)       │   Opus Encoded       │  (Host)          │
│                 │   Port 48100         │                  │
│ - PS Vita       │                      │ - Windows        │
│ - Windows       │                      │ - Linux          │
│ - Linux         │                      │                  │
└─────────────────┘                      └──────────────────┘
                                                  │
                                                  ▼
                                         ┌─────────────────┐
                                         │ Virtual Audio   │
                                         │ Device          │
                                         │ (WASAPI/Pulse)  │
                                         └─────────────────┘
```

## Project Structure

```
libmoonmic/
├── moonmic.h                    # Public C API
├── moonmic_internal.h           # Internal types
├── moonmic_client.cpp           # Main client implementation
├── CMakeLists.txt
├── README.md
├── INTEGRATION.md               # Integration guide
├── codec/
│   └── opus_encoder.cpp         # Opus encoding
├── network/
│   └── udp_sender.cpp           # UDP transmission
├── platform/                    # Platform-specific implementations
│   ├── psvita/
│   │   └── audio_capture_vita.cpp
│   ├── windows/
│   │   └── audio_capture_windows.cpp
│   └── linux/
│       └── audio_capture_linux.cpp
├── examples/
│   └── integration_example.cpp
└── host/                        # Host application
    ├── CMakeLists.txt
    ├── README.md
    ├── driver/                  # VB-CABLE driver (Windows)
    └── src/
        ├── main.cpp
        ├── config.cpp
        ├── audio_receiver.cpp
        └── ...
```

## Features

### Client Library (libmoonmic)

- ✅ Simple C API
- ✅ Modular platform architecture
- ✅ Opus encoding (64 kbps mono / 96 kbps stereo)
- ✅ UDP transmission with packet validation
- ✅ Low latency (10ms frames @ 48kHz)
- ✅ Auto-start capability
- ✅ Error callbacks

### Host Application (moonmic-host)

- ✅ Cross-platform (Windows, Linux)
- ✅ Dear ImGui GUI + console mode
- ✅ Sunshine integration (paired clients whitelist)
- ✅ VB-CABLE driver integration (Windows)
- ✅ Virtual audio device injection
- ✅ Real-time statistics
- ✅ Automatic driver installation

## Quick Start

### Client Integration

```cpp
#include "moonmic.h"

moonmic_config_t config = {
    .host_ip = "192.168.1.100",
    .port = 48100,
    .sample_rate = 48000,
    .channels = 1,
    .bitrate = 64000,
    .auto_start = true
};

moonmic_client_t* mic = moonmic_create(&config);
// Transmitting automatically...

moonmic_destroy(mic);
```

### Host Application

```bash
cd host
mkdir build && cd build
cmake ..
make

# Run
./moonmic-host

# Install VB-CABLE driver (Windows only)
./moonmic-host --install-driver
```

## Audio Protocol

### Packet Format

```
┌──────────────┬──────────────┬──────────────┬─────────────┐
│ Magic (4B)   │ Sequence(4B) │ Timestamp(8B)│ Opus Data   │
│ 0x4D4D4943   │              │              │             │
└──────────────┴──────────────┴──────────────┴─────────────┘
```

### Audio Parameters

| Parameter | Value |
|-----------|-------|
| Sample Rate | 48000 Hz |
| Channels | 1 (mono) or 2 (stereo) |
| Frame Size | 480 samples (10ms @ 48kHz) |
| Bitrate | 64 kbps (mono) / 96 kbps (stereo) |
| Port | 48100 (configurable) |
| Codec | Opus (VOIP mode, CBR) |

## Platform Support

### Client Platforms

| Platform | Status | Audio API |
|----------|--------|-----------|
| PS Vita | ✅ Implemented | SceAudio |
| Windows | ✅ Implemented | WASAPI |
| Linux | ✅ Implemented | PulseAudio |
| macOS | 🔄 Ready for extension | AVFoundation |
| Android | 🔄 Ready for extension | AudioRecord |

### Host Platforms

| Platform | Status | Virtual Device |
|----------|--------|----------------|
| Windows | ✅ Implemented | VB-CABLE |
| Linux | ✅ Implemented | PulseAudio |

## Dependencies

### Client Library

- **Opus** (encoding)
- **Platform audio API** (SceAudio, WASAPI, PulseAudio)
- **Standard sockets** (UDP)

### Host Application

- **Opus** (decoding)
- **GLFW** (GUI, optional)
- **Dear ImGui** (GUI, optional)
- **nlohmann/json** (configuration)
- **VB-CABLE** (Windows virtual microphone)

## Building

See [INTEGRATION.md](INTEGRATION.md) for detailed build instructions.

### Quick Build (Linux)

```bash
# Client library
mkdir build && cd build
cmake ..
make libmoonmic

# Host application
cd host
mkdir build && cd build
cmake ..
make
```

## Documentation

- [INTEGRATION.md](INTEGRATION.md) - Integration guide for vita-moonlight
- [host/README.md](host/README.md) - Host application documentation
- [host/driver/README.md](host/driver/README.md) - VB-CABLE driver guide

## Use Cases

- **Remote Gaming**: Transmit voice chat from PS Vita to PC
- **Streaming**: Use Vita microphone with OBS/Discord on PC
- **Voice Chat**: Enable voice communication in Moonlight sessions
- **Testing**: Test microphone handling without physical hardware

## License

This project is part of vita-moonlight.

VB-CABLE driver (Windows) is donationware by VB-Audio Software:
- Free for end users
- Donations welcome at: https://vb-audio.com/Cable/

## Contributing

Contributions welcome! To add a new platform:

1. Create `platform/yourplatform/audio_capture_yourplatform.cpp`
2. Implement `audio_capture_t` interface
3. Add platform detection to `CMakeLists.txt`
4. Update documentation

## Credits

- **VB-Audio Software** - VB-CABLE virtual audio driver
- **Xiph.Org** - Opus audio codec
- **Dear ImGui** - Immediate mode GUI library
