# Steam Streaming Microphone Driver

## Overview

The **Steam Streaming Microphone** is a high-performance virtual audio driver developed by Valve Corporation for Steam Remote Play.

**MoonMic Host uses this driver as the RECOMMENDED option** because:
- **Low Latency**: It supports WDM-KS (Windows Driver Model Kernel Streaming), offering significantly lower latency than standard WASAPI devices.
- **Reliability**: Designed specifically for low-latency game streaming.
- **Simplicity**: Single endpoint ("Steam Streaming Microphone") for capture.

## Included Files

This directory contains the driver files extracted from Steam:

- `x64/` - 64-bit drivers (`SteamStreamingMicrophone.sys`, `.inf`, `.cat`)
- `x86/` - 32-bit drivers

## Installation

### Automatic (Recommended)

1. Run `moonmic-host.exe`.
2. Open **"Driver Manager"**.
3. Click **"Install Steam Streaming Microphone"**.
4. Follow the prompts (Admin rights required).
5. **Reboot** your computer.

### Manual Command Line

```batch
moonmic-host.exe --install-steam-driver
```

## Verification

After installation and reboot:

1. Open Windows **Sound Settings** -> **Recording** tab.
2. You should see: **Microphone (Steam Streaming Microphone)**.
3. Ensure it is **Enabled**.

> **Note**: A "Speakers (Steam Streaming Microphone)" device may also appear. MoonMic automatically disables this during setup to prevent confusion, as it is not used for audio injection.

## Usage

MoonMic Host writes audio directly to this driver using WDM-KS. 
Applications (Discord, OBS, etc.) simply select **"Microphone (Steam Streaming Microphone)"** as their input device.

## Uninstallation

1. Open `moonmic-host` **Driver Manager**.
2. Click **"Uninstall Steam Driver"**.
3. Reboot.

## Credits

- **Valve Corporation**: Developed and maintains this driver for Steam.
- MoonMic redistributes these drivers for compatibility with Moonlight clients.
