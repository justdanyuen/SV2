# SV2 — Surround Vocal Spectrum Visualizer

A real-time surround sound spatial and frequency visualizer plugin for choral and ensemble mixing in 5.1 surround sessions. Built with JUCE 8 as part of a Berklee College of Music culminating experience project.

---

## Overview

SV2 is designed for mixing engineers and music directors working with choral or ensemble recordings in a 5.1 surround format. It provides two synchronized visualization windows:

- **Surround Field** — a polar visualization showing the spatial energy of each voice group around the listener, with LFE contribution displayed as concentric rings
- **Frequency Spectrum** — a real-time spectrum analyzer (20Hz–10kHz) showing overlapping frequency curves per voice group with a summed master curve underneath

Up to six voice groups are supported — Soprano, Mezzo, Alto, Tenor, Baritone, and Bass — each color-coded and independently controllable.

---

## Features

- 5.1 multichannel input bus (L, R, C, LFE, Ls, Rs)
- Real-time polar surround field visualization with LFE rings
- Real-time frequency spectrum with dB scaling and smoothed RTA ballistics
- Per-group color coding: Soprano (purple), Mezzo (pink), Alto (blue), Tenor (teal), Baritone (amber), Bass (coral)
- Solo and show/hide controls per voice group
- Additive solo — any combination of groups can be soloed simultaneously
- Show All / Hide All global controls
- Summed master EQ curve overlaid on spectrum view
- Cross-instance shared memory bridge — multiple plugin instances communicate in real-time
- Stale slot detection with graceful curve decay on silence

---

## Supported Formats

| Format | Host |
|--------|------|
| AU | Logic Pro (primary) |
| VST3 | Reaper, other VST3 hosts |
| AAX | Pro Tools (requires developer license for unsigned builds) |
| Standalone | Testing and demonstration |

---

## Requirements

- macOS 10.11 or later (Apple Silicon + Intel universal binary)
- JUCE 8.0.12 (bundled via CPM)
- CMake 3.25 or later
- Xcode Command Line Tools

---

## Building

```bash
git clone https://github.com/justdanyuen/SV2.git
cd SV2
cmake --preset default
cmake --build cmake-build
```

Plugins are automatically copied to:
- AU → `~/Library/Audio/Plug-Ins/Components/`
- VST3 → `~/Library/Audio/Plug-Ins/VST3/`
- AAX → `/Library/Application Support/Avid/Audio/Plug-Ins/`

---

## Session Setup

### Logic Pro (Recommended)
1. Create a 5.1 surround session
2. Set up one Aux track per voice group routed to the 5.1 master bus
3. Insert **SV2** (Audio Units → WolfSound → SV2) on each Aux track
4. Set the **"This instance:"** dropdown to the corresponding voice group
5. Open one additional instance on the master bus or a monitoring track — this becomes the master viewer showing all groups simultaneously

### Pro Tools
1. Create 5.1 Aux tracks per voice group
2. Insert **SV2.aaxplugin** on each track
3. Set voice group assignment in the plugin UI
4. Note: AAX builds require a Pro Tools Developer license for unsigned plugin loading

---

## Architecture

SV2 uses a **satellite/master** pattern via cross-process shared memory:

```
Soprano Aux → [SV2 Satellite, slot 0] ─┐
Mezzo Aux   → [SV2 Satellite, slot 1] ─┤→ /tmp/sv2_shared.bin → [SV2 Master] → UI
Alto Aux    → [SV2 Satellite, slot 2] ─┘
...
```

Each satellite instance writes its 6-channel RMS levels and 1024-bin FFT data to a memory-mapped file. The master instance reads all slots and renders the combined visualization. A sequence lock (seqlock) ensures thread-safe reads and writes without blocking the audio thread.

---

## Controls

| Control | Description |
|---------|-------------|
| **This instance:** | Sets which voice group slot this instance writes to |
| **Show All** | Makes all groups visible and clears solos |
| **Hide All** | Hides all groups and clears solos |
| **show** (per group) | Toggles visibility of that group in both visualizers |
| **S** (per group) | Additive solo — solos this group alongside any others already soloed |
| Tab bar | Switches between Surround Field and Frequency Spectrum views |

---

## Project Structure

```
SV2/
├── CMakeLists.txt
├── surround_visualizer/
│   ├── include/Surround_Visualizer/
│   │   ├── SharedMemoryBridge.h      # Cross-process shared memory
│   │   ├── SampleFifo.h              # Thread-safe audio FIFO
│   │   ├── Parameters.h              # Plugin parameters
│   │   ├── PluginProcessor.h
│   │   ├── PluginEditor.h
│   │   ├── SurroundVisualizerComponent.h
│   │   └── SpectrumVisualizerComponent.h
│   └── source/
│       ├── PluginProcessor.cpp       # Audio processing + FFT analysis
│       ├── PluginEditor.cpp          # UI and controls
│       ├── SurroundVisualizerComponent.cpp  # Polar field drawing
│       └── SpectrumVisualizerComponent.cpp  # Spectrum drawing
└── libs/
    └── juce/                         # JUCE 8.0.12 (via CPM)
```

---

## Known Limitations

- DAW track mute does not hide curves — use the **show** button or **Hide All** in the plugin UI to hide groups during mixing
- AAX builds require an Avid developer license for testing without PACE code signing
- Shared memory initialization race condition may occasionally require session reload on first open
- Test mode animation (no audio connected) is functional in DAW context only

---

## Built With

- [JUCE 8.0.12](https://juce.com) — audio plugin framework
- [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) — CMake package manager
- AAX SDK (bundled with JUCE 8)

---

## Author

Justin Yuen  
Berklee College of Music — Culminating Experience Project  
[github.com/justdanyuen/SV2](https://github.com/justdanyuen/SV2)

---

## License

MIT License — see LICENSE file for details.
