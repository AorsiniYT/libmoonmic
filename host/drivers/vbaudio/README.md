# VB-CABLE Driver for MoonMic Host

> **Note**: This is the **Alternative** driver option.
> The **Steam Streaming Microphone** driver is recommended for lower latency.

## What is VB-CABLE?

VB-CABLE is a virtual audio driver that creates a virtual audio cable on Windows:

- **CABLE Input** (Playback Device) - Where moonmic-host writes audio
- **CABLE Output** (Recording Device) - Virtual microphone that applications can capture

```
moonmic-host → CABLE Input → CABLE Output → Discord/Teams/OBS/etc.
```

## Included Files

This directory contains the official VB-CABLE drivers:

- `VBCABLE_Setup_x64.exe` - Installer for Windows 64-bit
- `VBCABLE_Setup.exe` - Installer for Windows 32-bit
- `vbaudio_cable64_win10.sys` - Driver for Windows 10/11 x64
- `vbaudio_cable64arm_win10.sys` - Driver for Windows 10/11 ARM64
- `.inf` and `.cat` files - Installation metadata

## Installation

### Option 1: Automatic (Recommended)

```bash
moonmic-host --install-driver
```

This will run the appropriate installer with administrator privileges.

### Option 2: Manual

1. Run `VBCABLE_Setup_x64.exe` as Administrator
2. Follow the installation wizard
3. **Reboot your computer** (required)

## Verification

After installing and rebooting:

1. Open Windows **Sound Settings**
2. Under **Output**, you should see: **CABLE Input (VB-Audio Virtual Cable)**
3. Under **Input**, you should see: **CABLE Output (VB-Audio Virtual Cable)**

## Usage with MoonMic

moonmic-host automatically detects VB-CABLE:

```
[VirtualDevice] Found VB-CABLE: CABLE Input (VB-Audio Virtual Cable)
[VirtualDevice] Initialized with VB-CABLE Input
[VirtualDevice] Virtual microphone: CABLE Output
```

Now any application can capture from "CABLE Output" as if it were a real microphone.

## Using in Applications

### Discord

1. Settings → Voice & Video
2. Input Device → **CABLE Output (VB-Audio Virtual Cable)**

### OBS Studio

1. Sources → Add → Audio Input Capture
2. Device → **CABLE Output (VB-Audio Virtual Cable)**

### Microsoft Teams / Zoom

1. Audio Settings
2. Microphone → **CABLE Output (VB-Audio Virtual Cable)**

### Sunshine

If Sunshine supports microphone capture, select:
- Input Device → **CABLE Output (VB-Audio Virtual Cable)**

## Uninstallation

1. Run `VBCABLE_Setup_x64.exe` as Administrator again
2. Select "Uninstall"
3. Reboot your computer

## License

VB-CABLE is **donationware** by VB-Audio Software:
- ✅ Free for end users
- ✅ Redistribution allowed (unmodified package)
- ✅ Donations welcome at: https://vb-audio.com/Cable/

**Important**: VB-CABLE cannot be integrated into another installer without author permission.

## Technical Details

### Driver Information

- **Version**: 3.3.1.7 (October 2024)
- **Platforms**: Windows 10/11 (x64, ARM64)
- **Type**: WDM Audio Driver
- **Channels**: Up to 16 channels (Windows 10 x64)
- **Sample Rates**: 8 kHz to 192 kHz

### How It Works

VB-CABLE creates a kernel-level audio loopback:

1. Applications write to **CABLE Input** (playback endpoint)
2. Driver routes audio internally
3. Audio appears on **CABLE Output** (recording endpoint)
4. Applications capture from **CABLE Output**

This is a true kernel driver, not a userspace loopback.

## Troubleshooting

### Driver not appearing after installation

**Solution**: Ensure you rebooted after installation. The driver requires a reboot to load.

### "Access Denied" during installation

**Solution**: Run the installer as Administrator (right-click → Run as Administrator).

### Audio not routing through cable

**Solution**: 
1. Verify moonmic-host is writing to CABLE Input
2. Check Windows Sound Settings that CABLE Output is enabled
3. Restart the application capturing from CABLE Output

## More Information

- Official website: https://vb-audio.com/Cable/
- Documentation: See `readme.txt` in this directory
- Support: https://vb-audio.com/Services/contact.htm

## Credits

VB-CABLE is developed and maintained by:
- **Vincent Burel** (V.Burel)
- **VB-Audio Software**
- Copyright © 2010-2024 All rights reserved
