# High-Performance Windows Screen Recorder — Architectural Specification

## 1. System Vision & Constraints
This software is an ultra-low-overhead, native Windows screen recording engine designed to achieve near-zero performance degradation on games and 3D applications. 

Unlike general-purpose streaming tools (e.g., OBS Studio) which employ modular filter graphs, software scene blending, and CPU frame staging, this recorder implements a **direct, hardware-only, zero-copy GPU pipeline**:
- **Zero CPU Frame Readback**: Capture surfaces remain in VRAM throughout capture, color space conversion, tone-mapping, and hardware encoding.
- **Hardware-Only Encoding**: Exclusively utilizes Media Foundation Transforms (MFT) backed by GPU vendors (NVENC, Intel QuickSync, AMD AMF).
- **Crash-Resilient Capture**: Records directly to Matroska (MKV) clusters and auto-remuxes to MP4 on clean completion.
- **Pure Native Execution**: Written in modern C++20 with a lightweight Direct2D/Win32 UI. No Electron, no .NET runtime, no garbage collection pauses.

---

## 2. Pipeline Flow (Capture -> GPU -> Encode -> Mux)

```
[ Display Surface / Window ]
             |
             v (WGC / DXGI OutputDuplication)
   [ ID3D11Texture2D ] (VRAM)
             |
             v (Zero-Copy Texture Share via D3D11 Device)
   [ Compositor / Crop / Tone-Map ]
             |
             v (IMFDXGIDeviceManager hardware surface handoff)
   [ Hardware MFT Encoder (NVENC / QSV / AMF) ]
             |
             v (Encoded H.264 / HEVC / AV1 NAL Packets)
   [ SPSC Lock-Free Ring Buffer ]
             |
             +----------------------------+
             |                            | (If Instant Replay Enabled)
             v                            v
   [ MKV Stream Writer ]         [ RAM Circular Replay Buffer ]
             |                            |
      (Recording Finalized)               v (Save Hotkey)
             |                   [ Dump to MP4/MKV ]
             v
   [ Fast In-Memory MP4 Remuxer ]
             |
   [ Finished .mp4 File ]
```

---

## 3. Capture Engine: WGC vs DXGI Decision Logic

### Windows.Graphics.Capture (WGC)
- **Primary Engine**: Windows 10 (1903+) and Windows 11.
- **Benefits**:
  - Captures any window (even occluded or minimized) without injecting DLLs.
  - Built-in hardware composition avoids desktop composition latency.
  - Native cursor removal (`IsCursorCaptureEnabled = false`).
  - Window exclusion API (`SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`).
  - Modern HDR10 / wide-color support.

### DXGI Desktop Duplication API (`IDXGIOutputDuplication`)
- **Fallback Engine**:
  - Activated for full-monitor capture on older Windows 10 versions where WGC is unavailable.
  - Activated for legacy DirectX 9/11 exclusive fullscreen applications where WGC presentation queue stalls.
- **Multi-Adapter Nuance**:
  - DXGI Desktop Duplication requires running on the specific `IDXGIAdapter` connected to the physical display output. The GPU subsystem manages inter-adapter texture sharing if encoding is configured on a secondary GPU.

---

## 4. Hardware Encoder Selection & Strict Fallback Policy

### Vendor Probing Sequence
1. **NVIDIA NVENC**: Clsid `CLSID_CMSH264EncoderMFT`, `CLSID_CMSHEVCEncoderMFT`, `CLSID_CMSAV1EncoderMFT`.
2. **Intel Quick Sync Video (QSV)**: Hardware MFT vendor GUID matching Intel (`0x8086`).
3. **AMD AMF / VCE**: Hardware MFT vendor GUID matching AMD (`0x1002`).

### Explicit Failure Policy (No Silent Degradation)
In accordance with system specifications, **the engine will never silently degrade to software (CPU) encoding**. 
- If hardware initialization fails (e.g. GPU session limits reached, unsupported codec, or driver crash):
  1. The pipeline halts immediately.
  2. An explicit diagnostic error is logged and presented to the user.
  3. The user must manually approve "Compatibility Software Mode" or select an alternate GPU.

---

## 5. Storage Pipeline: MKV Internal Format & Sub-Second MP4 Remux

### Why MKV Internally?
Standard MP4 files store key track metadata in the `moov` atom. If writing fails before the file is closed (e.g., game crash, BSOD, system power loss, or forced shutdown), the `moov` atom is never written, rendering the entire file corrupted.
MKV is an EBML-based container where video and audio data are written in self-contained **Clusters**. Each cluster can be played independently up to the exact millisecond of interruption.

### Lossless MP4 Remux
Upon clean recording termination:
1. The Matroska file is flushed and closed.
2. The `Mp4Remuxer` parses the EBML blocks, extracts raw NAL units and AAC frames, and builds an ISO Base Media File Format (MP4) structure.
3. The `moov` atom is placed at the front of the file (`faststart`) for instant streaming and editing software compatibility.
4. Duration: Pure sequential I/O (~1-3 seconds for multi-gigabyte files).

---

## 6. Instant Replay Buffer (ShadowPlay-style)

The Instant Replay subsystem maintains a circular buffer in system RAM of pre-encoded video/audio packets:
- **Zero Overhead**: Because frames are already encoded by the hardware MFT, RAM consumption is tiny (~3.75 MB/sec at 30 Mbps, or ~450 MB for a 2-minute buffer).
- **IDR Alignment**: Packets are indexed by keyframes. When the user hits the hotkey, the buffer writes packets from the earliest keyframe within the buffer time directly to disk.

---

## 7. Threading & Synchronization Model

| Thread Name | Priority | Thread Function |
| :--- | :--- | :--- |
| **UI Thread** | Normal | Win32 message pump, Direct2D UI rendering, tray icon, hotkey registration. |
| **Capture & Compositor** | Time Critical (MMCSS: `DisplayPostProcessing`) | Pumps WGC/DXGI frames, applies viewport crop/watermark, submits D3D11 surface to MFT. |
| **Audio Pump** | Time Critical (MMCSS: `Audio`) | WASAPI system loopback + mic capture, float32 mixing, MFT AAC encoding. |
| **Disk Muxer** | Above Normal | Consumes SPSC lock-free packet queue, writes EBML clusters to disk. |
| **Telemetry & Worker** | Below Normal | Remuxing worker, performance monitor (CPU, GPU load, dropped frames), process watcher. |
