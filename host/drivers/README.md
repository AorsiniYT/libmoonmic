# MoonMic Host - Virtual Audio Drivers

MoonMic Host requires a virtual audio driver to inject received audio into Windows as a virtual microphone provided to applications.

## Available Drivers

### 1. Steam Streaming Microphone (Recommended)
- **Path**: [`SVACDriver/`](SVACDriver/README.md)
- **Type**: WDM-KS Kernel Streaming
- **Latency**: Ultra Low
- **Developer**: Valve Corporation
- **Best for**: Gaming, VOIP, low-latency applications.

### 2. VB-CABLE (Alternative)
- **Path**: [`vbaudio/`](vbaudio/README.md)
- **Type**: Standard WDM Audio Device
- **Latency**: Standard
- **Developer**: VB-Audio Software
- **Best for**: Compatibility with older software that doesn't support WDM-KS exclusive modes.

## Deployment

The `moonmic-host.exe` application embeds compressed archives of these drivers and extracts them on demand. You do **not** need to manually install them from this folder unless debugging.

Use the built-in **Driver Manager** in the application GUI to install or switch drivers.
