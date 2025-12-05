# MoonMic Host Application

Cross-platform host application for receiving microphone audio from Moonlight clients.

## Overview

**moonmic-host** receives UDP audio packets from libmoonmic clients, decodes them, and injects the audio into a virtual audio device on the host system. This allows any application (Discord, Teams, OBS, etc.) to capture the remote microphone as if it were a local device.

## Features

- ✅ **Cross-platform**: Windows and Linux support
- ✅ **Virtual Microphone**: VB-CABLE integration (Windows) or PulseAudio (Linux)
- ✅ **GUI + Console**: Dear ImGui interface or headless mode
- ✅ **PairStatus Validation**: Certificate-based client authentication
- ✅ **Handshake Protocol**: Secure pre-stream client verification
- ✅ **Client Name Display**: Shows connected device identifier
- ✅ **Sunshine Integration**: Automatic client whitelist from Sunshine paired devices
- ✅ **Real-time Stats**: Packets received, dropped, bandwidth, sender IP, client name
- ✅ **Auto-installation**: VB-CABLE driver installer (Windows)
- ✅ **Low Latency**: Opus decoding with adaptive buffering

## How It Works

```
Client (Vita) → UDP → moonmic-host → Virtual Device → Apps
                                      (VB-CABLE)      (Discord/OBS)
```

### Windows (VB-CABLE)

```
moonmic-host → CABLE Input → CABLE Output → Discord/Teams/OBS
              (playback)     (virtual mic)
```

### Linux (PulseAudio)

```
moonmic-host → PulseAudio → null-sink → Applications
              (playback)    (virtual mic)
```

## Requirements

### Windows

- Windows 10/11 (x64 or ARM64)
- VB-CABLE driver (included in `driver/` folder)
- Opus library
- GLFW (for GUI)

### Linux

- PulseAudio
- Opus library
- GLFW (for GUI)

## Installation

### Windows

1. **Build or download** moonmic-host executable
2. **Install VB-CABLE driver**:
   ```bash
   moonmic-host --install-driver
   # Or manually run driver/VBCABLE_Setup_x64.exe as Administrator
   ```
3. **Reboot** your computer (required by driver)
4. **Run** moonmic-host:
   ```bash
   moonmic-host
   ```

### Linux

```bash
# Install dependencies
sudo apt-get install libopus-dev libpulse-dev libglfw3-dev

# Build
mkdir build && cd build
cmake ..
make

# Run
./moonmic-host
```

## Usage

### GUI Mode (Default)

```bash
./moonmic-host
```

Shows real-time interface with:
- Sunshine integration status
- Paired clients list
- Connected client name and validation status
- Reception statistics (packets, bytes, latency)
- Configuration options (whitelist, port, etc.)
- Start/Stop controls

### Console Mode

```bash
./moonmic-host --no-gui
```

Runs in headless mode with periodic stats output.

### Custom Configuration

```bash
./moonmic-host --config /path/to/config.json
```

### Driver Installation (Windows)

```bash
./moonmic-host --install-driver
```

Automatically installs VB-CABLE driver with UAC elevation.

## Configuration

Default config location:
- **Windows**: `%APPDATA%\AorsiniYT\MoonMic\moonmic-host.json`
- **Linux**: `~/.config/AorsiniYT/MoonMic/moonmic-host.json`

Example configuration:

```json
{
  "server": {
    "port": 48100,
    "bind_address": "0.0.0.0"
  },
  "audio": {
    "virtual_device_name": "MoonMic Virtual Microphone",
    "sample_rate": 48000,
    "channels": 1,
    "buffer_size_ms": 20
  },
  "security": {
    "enable_whitelist": true,
    "sync_with_sunshine": true,
    "allowed_clients": []
  },
  "NOTE": "When enable_whitelist=true, only clients with PairStatus=1 are accepted"
  "gui": {
    "show_on_startup": true,
    "theme": "dark"
  }
}
```

## Client Validation

moonmic-host validates clients using the **PairStatus handshake protocol**:

1. **Client validates with Sunshine** (via HTTPS `/serverinfo` with client certificate)
2. **Client sends handshake** to moonmic-host with `PairStatus`
3. **Host validates handshake**:
   - `enable_whitelist=false` → Accept all clients
   - `enable_whitelist=true` + `PairStatus=1` → Accept (paired)
   - `enable_whitelist=true` + `PairStatus≠1` → Reject (unpaired)

**Important**: The host does NOT query `sunshine_state.json` or Sunshine APIs for validation. All validation is based on the PairStatus sent by the client in the handshake packet.

## VB-CABLE Driver (Windows)

### What is VB-CABLE?

VB-CABLE creates a virtual audio cable with two endpoints:
- **CABLE Input** (Playback Device) - Where moonmic-host writes audio
- **CABLE Output** (Recording Device) - Virtual microphone that apps capture

### Installation

Included in `driver/` folder:
- `VBCABLE_Setup_x64.exe` - Windows 64-bit installer
- `VBCABLE_Setup.exe` - Windows 32-bit installer
- Driver files (.sys, .inf, .cat)

### Verification

After installation and reboot:

1. Open **Sound Settings**
2. **Output devices**: Should show "CABLE Input (VB-Audio Virtual Cable)"
3. **Input devices**: Should show "CABLE Output (VB-Audio Virtual Cable)"

### Using Virtual Microphone

**Discord**:
1. Settings → Voice & Video
2. Input Device → **CABLE Output (VB-Audio Virtual Cable)**

**OBS**:
1. Sources → Audio Input Capture
2. Device → **CABLE Output (VB-Audio Virtual Cable)**

**Teams/Zoom**:
1. Audio Settings
2. Microphone → **CABLE Output (VB-Audio Virtual Cable)**

## Building

### Windows (MinGW)

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

### Linux

```bash
mkdir build && cd build
cmake ..
make
```

### Dependencies

Install via package manager or vcpkg:
- Opus: `libopus-dev` or `opus:x64-windows`
- GLFW: `libglfw3-dev` or `glfw3:x64-windows`
- nlohmann/json: Header-only (included)

## Distribution

Package structure:

```
moonmic-host-windows/
├── moonmic-host.exe
├── moonmic-host.json (default config)
├── driver/
│   ├── README.md
│   ├── VBCABLE_Setup_x64.exe
│   └── ... (VB-CABLE files)
└── README.md
```

## Troubleshooting

### Windows: VB-CABLE not detected

```
[VirtualDevice] VB-CABLE not detected
[VirtualDevice] Please install VB-CABLE from driver/ folder
```

**Solution**: Run `moonmic-host --install-driver` and reboot.

### Linux: PulseAudio error

```
[VirtualDevice] PulseAudio error: Connection refused
```

**Solution**: Ensure PulseAudio is running: `pulseaudio --check`

### No audio in applications

**Windows**: Verify CABLE Output is selected as microphone in app settings.  
**Linux**: Check PulseAudio mixer: `pavucontrol`

## Statistics

GUI shows real-time stats:
- Packets received/dropped
- Bytes transferred
- Sender IP address
- Connected client name
- Client validation status (PairStatus)
- Reception status
- Sunshine paired clients

## Sunshine Web UI Integration (Optional)

The host application includes Sunshine Web UI integration for **debugging and GUI features only**:

- **Display paired clients** in Sunshine Settings GUI
- **Monitor Sunshine status** in main GUI
- **NOT used for client validation** - Validation uses PairStatus handshake

**To use Web UI features:**
1. Click "Login to Sunshine Web UI" in GUI
2. Enter Sunshine Web UI credentials
3. View paired clients list in Sunshine Settings

**Note**: This is optional. Client validation works without Web UI login.

## License

Part of vita-moonlight project.

**VB-CABLE** (Windows driver) is donationware by VB-Audio Software:
- Free for end users
- Redistribution allowed (unmodified)
- Donations: https://vb-audio.com/Cable/

## Credits

- **VB-Audio Software** - VB-CABLE virtual audio driver
- **Dear ImGui** - GUI library
- **Xiph.Org** - Opus codec
