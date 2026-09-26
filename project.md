# Master Prompt: High-Performance Windows Screen Recorder

## Project Summary
Build a native Windows desktop application that does **strictly screen recording and screen capture** — no editing, no streaming/broadcast features. The single most important constraint is that it must have **near-zero performance impact on running games/apps**, noticeably better than OBS Studio, by using GPU-native capture and hardware encoding exclusively. It must ship as a real Windows application (installer + optional portable build), with clean, minimal, well-segregated UI and a well-documented, maintainable codebase.

---

## 1. Tech Stack (chosen for performance — do not substitute without strong justification)

- **Language: C++17/20**, not C#/.NET. The performance requirement is non-negotiable, and native C++ avoids CLR/GC overhead and WinRT projection marshaling cost in the hot capture/encode path.
- **Capture APIs:**
  - `Windows.Graphics.Capture` (WGC) as the primary engine for window, app, and monitor capture — GPU-accelerated, no process injection.
  - DXGI Desktop Duplication API as a fallback/alternative for full-monitor capture scenarios where WGC has limitations.
- **Encoding:** Media Foundation Transform (MFT) hardware encoders only — NVENC (NVIDIA), Quick Sync (Intel), VCE/AMF (AMD). No software x264 encoding path except as an explicit, clearly-labeled "compatibility mode" the user must opt into, with a warning about performance impact.
- **GPU pipeline:** Direct3D 11 (or 12) for zero-copy texture handoff from capture → encoder, avoiding CPU readback wherever possible.
- **UI framework:** Win32 + Direct2D/DirectComposition, or WinUI 3, for a native, lightweight, GPU-composited UI — avoid Electron/web-based UI shells entirely; they conflict with the low-overhead goal and bloat the install.
- **Audio:** WASAPI loopback capture for system audio, WASAPI for mic, and Windows' per-process audio capture API for per-app audio isolation.
- **Build system:** CMake, targeting x64 (and optionally ARM64 for Windows-on-ARM).
- **Packaging:** MSIX or a signed MSI/EXE installer, plus a separate portable single-EXE build with no install/registry footprint.

State explicitly in the brief to the agent: *do not propose Electron, .NET/WPF, or any interpreted/managed stack as a substitute — GPU-native C++ is a hard requirement, not a preference.*

---

## 2. Core Capture Requirements
- Strictly screen/window/region capture — no editing timeline, no upload/streaming integration.
- Capture source selection: full monitor, specific application, specific window, or custom-drawn region/rectangle.
- Multi-monitor support: list and select any connected monitor individually.
- **Simultaneous multi-monitor recording** into independent output files.
- "Follow active window" mode that automatically tracks focus across monitors/windows.
- Custom resolution (including downscale/upscale from source) and custom FPS, with sane presets (720p/1080p/1440p/4K, 30/60/120/144 fps) plus fully manual entry.
- Constant Frame Rate (CFR) vs Variable Frame Rate (VFR) toggle, explained in UI (CFR = safer for later editing, VFR = smaller files).

## 3. Performance Requirements (highest priority)
- Zero/minimal impact on game FPS — hardware-encode only, GPU-to-GPU pipeline, no unnecessary CPU texture copies.
- On multi-GPU (laptop) systems, allow explicit selection of which GPU performs capture + encode, independent of which GPU renders the game.
- Live performance overlay (optional, toggleable) showing: recorder's own CPU%, GPU encode load, current bitrate, and dropped-frame count — so users can distinguish "game is lagging" from "recorder is lagging."
- Dropped-frame logging, visible to the user, not silently swallowed.
- If hardware encoding is unavailable, explicitly warn the user rather than silently degrading to software encode.

## 4. Recording Modes
- Standard start/stop recording.
- Pause/resume without terminating the output file.
- **Instant replay buffer**: continuously ring-buffer the last N seconds/minutes (user-configurable, RAM-based), saved to disk on a dedicated hotkey — ShadowPlay-style.
- Auto-split recordings by max file size or max duration.
- Scheduled recording: start/stop at specified times.
- Auto-start/stop tied to a specified process (e.g., auto-record when `game.exe` launches, stop when it exits).
- Command-line flags / a minimal local API to start/stop recording programmatically, for power users and scripting.

## 5. Audio
- System audio loopback capture.
- Microphone capture.
- Per-application audio capture (isolate audio from one specific app/game).
- Independent volume/mute control per audio source, mixed at record time.
- Live mic mute/unmute via hotkey.

## 6. Output & Encoding Control
- Codec choice: H.264 (max compatibility), H.265/HEVC and AV1 where the hardware encoder supports it (smaller files).
- Bitrate mode: CBR / VBR / CQP with sensible defaults and manual override.
- Recording container: record to **MKV internally** (survives crashes/power loss without corrupting), with **auto-remux to MP4** on clean finalize for compatibility. Document this design decision in the code/README.
- HDR capture support (HDR10/Auto HDR) with correct tone-mapping to SDR when the chosen output format requires it; correct color format handling (NV12/P010) tied to codec.
- Custom output path, plus filename templating (date, time, source app name, resolution, etc.).
- Low-disk-space warning before it becomes critical mid-recording.

## 7. Hotkeys
All independently rebindable, with sane defaults:
- Start/stop recording
- Pause/resume
- Save replay buffer clip
- Screenshot
- Mute/unmute mic
- (Optional) Add marker/bookmark during recording

## 8. On-Screen Presence & Notifications
- Small, unobtrusive on-screen recording indicator with elapsed time — **excluded from the capture itself**.
- Optional keystroke/mouse-click visualizer overlay (toggleable) for tutorial-style content.
- Optional webcam picture-in-picture overlay, resizable and repositionable.
- Optional watermark/logo overlay on output.
- System tray residency: app can run start-minimized and be fully controlled from the tray icon without a window open.
- Toast/notification on save with an "Open folder" quick action.
- Timestamped markers/bookmarks placed via hotkey during recording, exported as a sidecar file or embedded chapter metadata (no editor needed to make use of these later).

## 9. Privacy & Capture Control
- Cursor visibility toggle; optional click highlighting.
- Ability to exclude specific windows/apps from capture during full-screen recording (e.g., notification popups, password managers).

## 10. File & Library Management
- In-app recordings library/history: list past recordings with thumbnail preview, quick open, rename, delete.
- Auto-delete recordings older than X days or exceeding Y total GB (configurable, off by default).
- Quick share action (e.g., copy file to clipboard for pasting into apps like Discord).

## 11. Config, Presets & Diagnostics
- Named, savable/loadable presets (e.g., "Gaming – Low Overhead," "Tutorial – High Quality").
- Config import/export, to move settings between machines.
- Local crash/error logging with a "Copy diagnostic report" button for bug reports.
- Optional, user-controlled update check (never silent/forced).

## 12. UI/UX Requirements
- Minimal, clean, properly segregated UI — clearly separate panels/sections for: Source selection, Video settings, Audio settings, Hotkeys, Output/Path, Library/History, and Advanced/Diagnostics. No dumping everything into one screen.
- Full keyboard navigability of the UI (not just recording hotkeys).
- Scalable UI/text for high-DPI displays.
- Dark/light theme, following the Windows system setting by default.

## 13. Explicitly Out of Scope (state this to the agent to prevent scope creep)
- No built-in video editing or trimming beyond simple marker metadata.
- No live streaming/broadcast integration (Twitch/YouTube Live, etc.).
- No cloud upload features.

## 14. Engineering & Delivery Requirements
- Deliver as a real Windows application: a signed installer (MSIX or MSI/EXE) **and** a portable single-EXE build with no install/registry footprint.
- Target x64, with ARM64 as a stretch goal.
- **Documentation required:**
  - Architecture overview document explaining the capture → GPU → encode → mux pipeline and why each major tech choice was made (WGC vs DXGI, MKV→MP4 remux strategy, hardware-encoder selection logic).
  - Inline code comments on all non-trivial logic, especially the capture/encode pipeline and threading model.
  - A README covering build instructions, dependencies, and how to add a new capture source or encoder backend.
  - A CONFIG/PRESETS doc describing the settings schema for anyone extending it later.
- **Code quality requirements:**
  - Modular, well-segregated architecture: capture layer, encode layer, audio layer, UI layer, and config/persistence layer should be cleanly separated with clear interfaces — no tangled monolith.
  - Consistent style guide (agent should pick and state one, e.g., a documented C++ style guide) and apply it uniformly.
  - Threading model must be explicit and documented (capture thread, encode thread, UI thread — no blocking the UI thread with capture/encode work).
  - Error handling must be explicit throughout (no silently swallowed failures, especially around hardware encoder availability and disk I/O).
  - Include basic automated tests where feasible (e.g., config serialization, filename templating, preset load/save) even though full capture-pipeline testing is hard to automate.

---

## Instruction to the Agent
Before writing code, propose the concrete architecture (module breakdown, threading model, and the WGC/DXGI decision logic per capture mode) and confirm the hardware-encoder fallback behavior, then proceed to implementation with full documentation and maintainable, modular code as described above.