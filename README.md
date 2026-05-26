# Clipper

OS-level screen recorder for creating social media content. Built with C++, Qt6, and FFmpeg.

Captures video at the GPU driver level via DXGI Desktop Duplication — bypassing browser DRM that blocks standard screen recording. Supports platform-specific aspect ratios (Instagram, YouTube, TikTok), system audio capture, real-time preview, and instant stream-copy trimming.

## Features

- **OS-level capture** — DXGI Desktop Duplication API captures the desktop framebuffer directly from the GPU. Works even on sites that block browser-based recording.
- **Platform presets** — one-click setup for aspect ratios and resolutions:
  - Instagram Post (1:1) — 1080×1080
  - Instagram Portrait (4:5) — 1080×1350
  - Instagram Story/Reel (9:16) — 1080×1920
  - YouTube (16:9) — 1920×1080
  - TikTok (9:16) — 1080×1920
- **Aspect-ratio-locked region selector** — transparent full-screen overlay with drag/resize/move
- **System audio capture** — WASAPI loopback captures audio output (not just microphone)
- **Real-time preview window** — always-on-top thumbnail of the capture feed
- **Recording indicator** — red blinking dot with elapsed timer (click-through overlay)
- **Global hotkey** — `Ctrl+Shift+R` to start/stop recording from anywhere
- **Video trimmer** — 2-handle timeline slider, exports via FFmpeg stream copy (near-instant, no re-encode)
- **Configurable quality** — FPS (10-60) and bitrate (2/4/8 Mbps)

## Building

### Prerequisites

- **Windows 10/11** with Visual Studio 2019 or later
- **CMake** 3.20+
- **Qt 6.x** — install via the [Qt Online Installer](https://www.qt.io/download) or vcpkg
- **FFmpeg** development libraries — install via [vcpkg](https://github.com/microsoft/vcpkg):
  ```powershell
  vcpkg install ffmpeg[core,avcodec,avformat,avutil,swscale,swresample]:x64-windows
  ```
- **ffmpeg.exe** on system PATH (required for the video trimmer)

### Build

```powershell
mkdir build
cd build
cmake .. `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64" `
  -DFFMPEG_ROOT="C:/path/to/vcpkg/installed/x64-windows"
cmake --build . --config Release
```

The executable will be at `build/Release/clipper.exe`.

### Quick build with vcpkg toolchain

```powershell
cmake .. `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64" `
  -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release
```

## Usage

1. Launch `clipper.exe`
2. Choose a platform preset from the dropdown
3. Click **Select Region** to pick the screen area to capture (aspect ratio is locked to the preset)
4. Optionally adjust FPS, quality, and output path
5. Click **Start Recording** (or press `Ctrl+Shift+R`)
6. A red recording indicator appears in the top-left; the preview window shows the live feed
7. Click **Stop Recording** (or press `Ctrl+Shift+R` again)
8. The trimmer dialog opens automatically — drag the handles to set in/out points, then click **Trim & Export**

## Architecture

```
src/
├── main.cpp                  Entry point
├── mainwindow.h/cpp          Main control panel UI
├── platformpresets.h         Aspect ratio definitions
├── screencapturer.h/cpp      DXGI desktop duplication capture
├── audiocapturer.h/cpp       WASAPI loopback audio capture
├── encoder.h/cpp             FFmpeg H.264 + AAC encoding
├── recorder.h/cpp            Multi-threaded orchestrator
├── regionselector.h/cpp      Aspect-ratio-locked overlay
├── hotkeymanager.h/cpp       Global hotkey (RegisterHotKey)
├── previewwindow.h/cpp       Real-time preview overlay
├── recordingindicator.h/cpp  Red dot + timer overlay
├── timelineslider.h/cpp      Two-handle range slider widget
└── videotrimmer.h/cpp        FFmpeg stream-copy trim dialog
```

## Limitations

- **HiDPI multi-monitor** — region selection uses the primary screen's scale factor. Mixed-DPI setups may need manual adjustment.
- **Stream-copy trimming** — keyframe-bound; trim points may shift slightly to the nearest keyframe. For frame-accurate cuts, re-encoding would be needed.
- **HDCP-protected content** — the DXGI API cannot bypass hardware-level copy protection on services like Netflix 4K.

## License

MIT
