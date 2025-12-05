# MoonMic Host - Quick Start Guide by AorsiniYT

## 1. Configuration (Automatic)

**The application creates configuration automatically on first run!**

Configuration is saved to:
- **Windows**: `%APPDATA%\AorsiniYT\MoonMic\moonmic-host.json`
- **Linux**: `~/.config/AorsiniYT/MoonMic/moonmic-host.json`

Settings automatically save when changed in the GUI. You rarely need to edit manually.

## 2. Install VB-CABLE (Windows Only - First Time)

**All driver files are embedded in `moonmic-host.exe`! No need for separate downloads.**

### Option A: GUI Installation (Easiest)
1. Run `moonmic-host.exe`
2. Look for the driver status section in the GUI
3. If VB-CABLE is not installed, click **"Install VB-CABLE Driver"** button
4. Click OK when prompted for admin rights
5. Wait for installation to complete
6. **Reboot your PC** (required)

### Option B: Command Line
```cmd
moonmic-host.exe --install-driver
```

The installer will:
- Extract embedded driver files to a temporary location
- Run the VB-CABLE installer
- Clean up temporary files automatically

### Verify Installation

After rebooting, check Windows Sound Settings:
- **Output**: You should see "CABLE Input (VB-Audio Virtual Cable)"
- **Input**: You should see "CABLE Output (VB-Audio Virtual Cable)"

> **Note**: The moonmic-host GUI will show "[OK] VB-CABLE Installed" if successful.

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
[AudioReceiver] Client validated: PS Vita (0123456789ABCDEF)
[AudioReceiver] Receiving audio...
```

The client name and uniqueid appear after successful handshake validation.

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

### Client Validation (Security)

moonmic-host uses **PairStatus-based validation** for security:

**How it works:**
1. Client calls Sunshine's `/serverinfo` endpoint (HTTPS + client certificate)
2. Sunshine returns PairStatus (1=paired, 0=unpaired)
3. Client sends handshake with PairStatus to moonmic-host
4. Host validates:
   - `enable_whitelist=false` → Accept all clients
   - `enable_whitelist=true` + `PairStatus=1` → Accept connection  
   - `enable_whitelist=true` + `PairStatus≠1` → Reject connection

**What you'll see:**
- Paired client: `[AudioReceiver] Client validated: PS Vita (uniqueid)`
- Unpaired client: `[AudioReceiver] DENY: Client not paired (PairStatus=0)`
- Whitelist disabled: `[AudioReceiver] Client connected: PS Vita [whitelist disabled]`

**Configuration:**
- Set `enable_whitelist: true` in config for strict security
- Set `enable_whitelist: false` for testing (accepts all clients)

### Sunshine Web UI (Optional GUI Feature)

The GUI includes Sunshine Web UI integration for **debugging only**:
- View list of paired clients in Sunshine Settings
- Monitor Sunshine connection status
- **NOT required** for client validation

To use: Click "Login to Sunshine Web UI" in the GUI and enter credentials.

## Important Files

- `moonmic-host.exe` - Main executable
- `config/moonmic-host.json.example` - Example configuration
- `driver/VBCABLE_Setup_x64.exe` - VB-CABLE installer
- `driver/README.md` - Complete driver documentation

## More Information

- VB-CABLE is free donationware: https://vb-audio.com/Cable/
- For Sunshine integration, see main `README.md`
