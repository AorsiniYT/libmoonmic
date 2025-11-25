# MoonMic Host - Quick Start Guide

## 1. Create Configuration File (Optional)

The program works with default settings, but if you want to customize:

**Windows:**
```cmd
mkdir "%APPDATA%\MoonMic"
copy config\moonmic-host.json.example "%APPDATA%\MoonMic\moonmic-host.json"
```

**Linux:**
```bash
mkdir -p ~/.config/moonmic
cp config/moonmic-host.json.example ~/.config/moonmic/moonmic-host.json
```

## 2. Install VB-CABLE (Windows Only)

### Option A: Automatic Installation
```cmd
moonmic-host.exe --install-driver
```

### Option B: Manual Installation

1. Navigate to the `driver/` folder
2. Run `VBCABLE_Setup_x64.exe` as Administrator
3. Follow the installation wizard
4. **Reboot your PC** (required)

### Verify Installation

After rebooting, check Windows Sound Settings:
- **Output**: You should see "CABLE Input (VB-Audio Virtual Cable)"
- **Input**: You should see "CABLE Output (VB-Audio Virtual Cable)"

## 3. Run moonmic-host

### Console Mode (Recommended for Testing)
```cmd
moonmic-host.exe --no-gui
```

### GUI Mode
```cmd
moonmic-host.exe
```

You should see:
```
[VirtualDevice] Found VB-CABLE: CABLE Input (VB-Audio Virtual Cable)
[VirtualDevice] Initialized with VB-CABLE Input
[AudioReceiver] Listening on 0.0.0.0:48100
```

## 4. Configure Applications

To use the virtual microphone in your applications:

### Discord
1. Settings → Voice & Video
2. Input Device → **CABLE Output (VB-Audio Virtual Cable)**

### OBS Studio
1. Sources → Add → Audio Input Capture
2. Device → **CABLE Output (VB-Audio Virtual Cable)**

### Microsoft Teams / Zoom
1. Audio Settings
2. Microphone → **CABLE Output (VB-Audio Virtual Cable)**

## 5. Connect from PS Vita

1. Open Moonlight on your PS Vita
2. Go to Settings
3. Enable "Enable Microphone" toggle
4. Start a streaming session
5. The microphone should connect automatically

You'll see in moonmic-host:
```
[AudioReceiver] Client connected: 192.168.x.x
[AudioReceiver] Receiving audio...
```

## Troubleshooting

### VB-CABLE not detected
- Ensure you rebooted after installation
- Check Windows Sound Settings to verify it appears

### Port 48100 in use
- Change the port in `moonmic-host.json`:
  ```json
  "server": {
    "port": 48101
  }
  ```
- Also update the port on PS Vita if possible

### No audio
- Check "CABLE Input" volume in Windows
- Ensure the application is capturing from "CABLE Output"
- Verify moonmic-host shows "[AudioReceiver] Receiving audio..."

### Sunshine Integration

moonmic-host automatically detects Sunshine and its paired clients for security.
If Sunshine is detected, only paired clients from `sunshine_state.json` can connect.

Location: `Sunshine\config\sunshine_state.json`

## Important Files

- `moonmic-host.exe` - Main executable
- `config/moonmic-host.json.example` - Example configuration
- `driver/VBCABLE_Setup_x64.exe` - VB-CABLE installer
- `driver/README.md` - Complete driver documentation

## More Information

- VB-CABLE is free donationware: https://vb-audio.com/Cable/
- For Sunshine integration, see main `README.md`
