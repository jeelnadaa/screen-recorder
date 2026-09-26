# Configuration & Presets Specification

## 1. Overview
The High-Performance Screen Recorder utilizes a lightweight JSON format for configuration and user presets. Settings are stored locally in `%LOCALAPPDATA%\ScreenRecorder\config.json`.

---

## 2. Configuration Schema

```json
{
  "version": 1,
  "video": {
    "capture_engine": "auto",       // "auto", "wgc", "dxgi"
    "adapter_index": 0,             // 0 = primary GPU, 1 = secondary GPU
    "resolution_width": 1920,
    "resolution_height": 1080,
    "fps": 60,
    "rate_mode": "CFR",             // "CFR" (Constant Frame Rate) or "VFR" (Variable Frame Rate)
    "codec": "H264",                // "H264", "HEVC", "AV1"
    "bitrate_mode": "CBR",          // "CBR", "VBR", "CQP"
    "target_bitrate_kbps": 30000,
    "max_bitrate_kbps": 40000,
    "cqp_quality": 20,              // 18-28 typical (lower = higher quality)
    "hdr_tone_mapping": true,
    "color_format": "NV12",         // "NV12" for 8-bit, "P010" for 10-bit HDR
    "allow_software_fallback": false // Explicit opt-in only
  },
  "audio": {
    "system_audio_enabled": true,
    "system_audio_volume": 1.0,
    "mic_enabled": true,
    "mic_device_id": "default",
    "mic_volume": 1.0,
    "per_process_audio_enabled": false,
    "target_process_name": "",
    "audio_codec": "AAC",
    "audio_bitrate_kbps": 192,
    "sample_rate": 48000,
    "channels": 2
  },
  "output": {
    "destination_directory": "D:\\Recordings",
    "filename_template": "{date}_{time}_{app}_{res}_{fps}",
    "record_container": "MKV",
    "auto_remux_mp4": true,
    "delete_mkv_after_remux": true,
    "auto_split_size_mb": 0,        // 0 = disabled
    "auto_split_duration_min": 0,   // 0 = disabled
    "low_disk_warning_mb": 5120     // 5 GB threshold
  },
  "replay": {
    "enabled": false,
    "buffer_duration_seconds": 60,
    "max_ram_mb": 1024
  },
  "hotkeys": {
    "start_stop": "Ctrl+Shift+R",
    "pause_resume": "Ctrl+Shift+P",
    "save_replay": "Ctrl+Shift+S",
    "screenshot": "Ctrl+Shift+F12",
    "mute_mic": "Ctrl+Shift+M",
    "add_marker": "Ctrl+Shift+B"
  },
  "overlays": {
    "show_recording_indicator": true,
    "show_performance_stats": true,
    "show_cursor": true,
    "highlight_clicks": false,
    "webcam_pip_enabled": false,
    "webcam_device_id": "",
    "webcam_position": "bottom_right", // "top_left", "top_right", "bottom_left", "bottom_right"
    "webcam_scale": 0.2
  },
  "process_triggers": {
    "auto_record_process": "",
    "stop_on_process_exit": true
  }
}
```

---

## 3. Filename Template Tokens

The output filename template supports dynamic tokens resolved at the time recording starts:

| Token | Description | Example Replacement |
| :--- | :--- | :--- |
| `{date}` | Current date in ISO-like format | `2026-09-26` |
| `{time}` | Current time in 24h format | `19-30-45` |
| `{timestamp}` | UNIX epoch timestamp | `1790431845` |
| `{app}` | Sanitized name of the target application/window | `Cyberpunk2077` |
| `{res}` | Video resolution | `1920x1080` |
| `{fps}` | Target frames per second | `60fps` |
| `{codec}` | Video codec name | `H264` |
| `{gpu}` | Active GPU vendor | `NVIDIA` |

---

## 4. Built-in Factory Presets

### Preset 1: "Gaming — Low Overhead" (Default)
- **Engine**: Auto (WGC primary)
- **Resolution**: Native display resolution
- **FPS**: 60
- **Codec**: H.264 / HEVC hardware MFT
- **Bitrate Mode**: CQP (Quality: 20) — minimizes encoder CPU feedback loops
- **Audio**: System loopback + Mic, AAC 192 kbps
- **Container**: MKV -> Auto-remux MP4

### Preset 2: "Tutorial — High Quality"
- **Engine**: WGC (Window or Monitor)
- **Resolution**: 1080p
- **FPS**: 60 CFR
- **Codec**: H.264 hardware MFT
- **Bitrate Mode**: CBR 25,000 kbps
- **Overlays**: Click highlighting enabled, Cursor visible
- **Audio**: System + Mic with noise gate/volume normalization

### Preset 3: "High-Framerate Esports (120/144 FPS)"
- **Engine**: WGC / DXGI
- **Resolution**: 1080p / 1440p
- **FPS**: 120 / 144
- **Codec**: HEVC / AV1 hardware MFT (reduces PCI-e bus and VRAM bandwidth)
- **Bitrate Mode**: VBR 45,000 kbps (Peak 60,000 kbps)
