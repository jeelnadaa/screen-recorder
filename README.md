# High-Performance Native Windows Screen Recorder

A state-of-the-art native Windows screen recording desktop application built from the ground up for **near-zero performance footprint** during high-demand 3D gaming and workstation tasks.

Exclusively uses GPU-native capture (`Windows.Graphics.Capture` and `DXGI Desktop Duplication`) and Direct3D 11 hardware-accelerated Media Foundation Transforms (NVENC, Intel QuickSync, AMD AMF).

---

## Key Features
- **GPU-Native Pipeline**: Direct3D 11 surface handoff directly into hardware encoder. Zero CPU frame copying or readbacks.
- **Ultra-Low Overhead**: Noticeably lighter than OBS Studio with no modular filter graph or unnecessary software conversions.
- **Hardware-Only Encoding**: Supports H.264, HEVC (H.265), and AV1 via MFT hardware backends. Strictly refuses silent CPU degradation.
- **Crash-Resilient Recording**: Records internally to Matroska (MKV) to protect against crashes/power loss, with sub-second automatic stream-copy remuxing to MP4 upon completion.
- **ShadowPlay-Style Instant Replay**: Configurable RAM circular packet buffer holding pre-encoded compressed packets, flushed instantaneously to disk on hotkey.
- **Advanced Audio**: System audio WASAPI loopback, microphone capture, and Windows per-process audio isolation with live mixing.
- **Native Direct2D / Win32 UI**: Pure C++20 fluent dark UI. Less than 20 MB RAM footprint. Zero Electron or web runtime bloat.
- **Privacy & Presence**: Excluded on-screen recording HUD (`SetWindowDisplayAffinity`), custom region capture, cursor visibility toggle, and window exclusion filters.

---

## Architecture Overview

```
[ WGC / DXGI Capture ] -> [ Direct3D 11 Surface ] -> [ Hardware MFT (NVENC/QSV/AMF) ]
                                                                |
                                             +------------------+------------------+
                                             |                                     |
                                    [ MKV Stream Writer ]                [ RAM Replay Buffer ]
                                             |                                     |
                                  (Auto MP4 Stream-Copy)                    (Instant Dump)
```

For detailed architectural diagrams and design rationale, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
For settings schema and presets, see [docs/CONFIG.md](docs/CONFIG.md).

---

## Build Prerequisites & Toolchain

### Prerequisites
1. **Operating System**: Windows 10 (version 1903 or later) or Windows 11 (64-bit).
2. **Compiler**: Microsoft Visual C++ (MSVC) from Visual Studio 2022 (v143 toolset) or Clang-cl with C++20 support.
3. **Windows SDK**: Windows 10/11 SDK (version 10.0.19041.0 or newer).
4. **Build System**: CMake 3.20 or newer.

### Building via CMake

```powershell
# Generate Visual Studio 2022 project files
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build release executable
cmake --build build --config Release

# Output executable located at:
# build/bin/Release/ScreenRecorder.exe
```

---

## How to Extend the Application

### 1. Adding a New Capture Source
Implement the `ICaptureEngine` interface defined in `include/capture/ICaptureEngine.h`:
```cpp
namespace Recorder::Capture {
    class ICustomCaptureEngine : public ICaptureEngine {
    public:
        virtual bool Initialize(ID3D11Device* d3dDevice) = 0;
        virtual bool StartCapture(const CaptureSourceDescriptor& source) = 0;
        virtual void StopCapture() = 0;
        virtual void SetFrameCallback(FrameCallback callback) = 0;
    };
}
```
Register the new engine in `SourceManager::RegisterEngine(...)`.

### 2. Adding a New Encoder Backend
Implement the `IVideoEncoder` interface defined in `include/encode/IVideoEncoder.h`:
```cpp
namespace Recorder::Encode {
    class ICustomVideoEncoder : public IVideoEncoder {
    public:
        virtual bool Initialize(const EncoderConfig& config, ID3D11Device* d3dDevice) = 0;
        virtual bool SubmitFrame(ID3D11Texture2D* texture, int64_t timestampQpc) = 0;
        virtual void Drain() = 0;
    };
}
```
Register the encoder in `MftVideoEncoder::ProbeEncoders(...)`.

---

## License & Attribution
Proprietary / MIT. Developed with modern C++20 for high-performance Windows capture.
