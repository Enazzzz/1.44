# 1.44

ASR-10-era constraint sampler. A CLAP instrument built raw against the official [CLAP SDK](https://github.com/free-audio/clap) — no JUCE.

Load WAV or MP3, select a region, and commit it only if the 16-bit capture would have fit in the memory budget. Playback is MIDI-triggered and pitch-shifts by changing playback rate (chipmunk / demonic, period-accurate). Loop points are naive: no automatic crossfade.

## Features (v1)

- WAV and MP3 import, decoded to linear PCM
- Zoomable / scrollable waveform with draggable region (`IN`/`OUT`) and loop (`LS`/`LE`) markers
- Audition of the selected region (optional preview loop) and waveform scrub
- Memory budget (default **1.44 MB = 1,474,560 bytes**, the 1440 KiB floppy), live size vs budget while dragging markers
- Sample rate: **44.1 kHz** or **29.76 kHz** (real ASR-10 options)
- Fixed **16-bit** depth
- Over-budget selections are **rejected** — never silently truncated
- Optional mono downmix
- Optional snap-to-zero-crossing on loop points
- Loop modes: one-shot, forward, ping-pong
- MIDI instrument: root note at captured pitch/speed; other notes change rate
- ADSR amplitude envelope

Out of scope: time-stretch, multi-zone mapping, effects.

## Build (Linux)

Dependencies: CMake ≥ 3.16, a C++20 compiler, libX11, pthread. Tests also use `ffmpeg` (for the MP3 fixture).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

This produces:

| Artifact | Purpose |
|---|---|
| `build/one44.clap` | CLAP plugin (shared module) |
| `build/one44_tests` | Constraint, pitch, decoder, and playback unit tests |
| `build/one44_clap_probe` | Headless instantiate/process check |

Install into the user CLAP path:

```bash
cmake --build build --target install-user
# copies build/one44.clap → ~/.clap/one44.clap
```

Or copy manually:

```bash
mkdir -p ~/.clap
cp build/one44.clap ~/.clap/one44.clap
```

## REAPER — Linux (trial)

This plugin is a **CLAP instrument**. In REAPER that means a track FX slot, not an audio insert you feed files into.

1. Install the official Linux trial from [reaper.fm/download.php](https://www.reaper.fm/download.php) (no license key; do not use cracks).
2. Put `one44.clap` on a path REAPER scans:
   - `~/.clap/one44.clap` (CLAP default, recommended)
   - and/or Preferences → Plug-ins → CLAP → additional scan path
3. Options → Preferences → Plug-ins → CLAP → **Rescan**.
4. FX browser name: **1.44** (identifier `com.enazzzz.one-four-four`).
5. Track setup:
   - Insert a new track
   - Click **FX** → CLAP → **1.44** (verified FX name: `CLAPi: 1.44 (1.44)`; Add-FX name `CLAP:1.44`)
   - Arm the track for MIDI (record-arm + input monitoring) or add a Virtual MIDI Keyboard
   - Open the plugin GUI: **Load** a WAV/MP3, drag **IN/OUT**, confirm the memory readout fits, **Commit**, then play notes
   - Root note (default C4 / MIDI 60) plays at the captured speed; other keys change playback rate

Verified on the official REAPER 7.79 Linux x86_64 trial (no license): after copying `one44.clap` to `~/.clap`, `TrackFX_AddByName(..., "CLAP:1.44")` instantiates the plugin as a CLAP instrument. REAPER's scan cache records:

```
[one44.clap]
com.enazzzz.one-four-four=1|1.44 (1.44)
```

Headless scan check (no GUI session required):

```bash
# After copying to ~/.clap
~/.clap/../  # plugin lives at ~/.clap/one44.clap
./build/one44_clap_probe ~/.clap/one44.clap
```

REAPER itself, from its install directory:

```bash
# Example after unpacking the official tarball
./reaper -nonewinst
```

Then Preferences → Plug-ins → CLAP → Rescan, and confirm **1.44** appears under CLAP instruments. A ReaScript that reports whether the plugin is registered:

```lua
-- reaper-check-one44.lua  (Actions → Load ReaScript)
local ok = false
-- REAPER enumerates FX by name once scanned; try adding it to track 1.
reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
local fx = reaper.TrackFX_AddByName(track, "CLAP:1.44", false, -1)
if fx >= 0 then
  reaper.ShowConsoleMsg("1.44 instantiated as CLAP instrument on track 1\n")
  ok = true
else
  reaper.ShowConsoleMsg("1.44 not found. Rescan CLAP paths (need ~/.clap/one44.clap).\n")
end
```

Run from the shell if you have a REAPER install and a display or a dummy X server:

```bash
reaper -nonewinst "$(pwd)/scripts/reaper-check-one44.lua"
```

## REAPER — Windows

A Windows `.clap` is not built in this tree (the GUI backend is X11). If you add a Win32 window backend, install to:

- `%LOCALAPPDATA%\Programs\Common\CLAP\one44.clap`
- or `%COMMONPROGRAMFILES%\CLAP\one44.clap`
- or a path added under Preferences → Plug-ins → CLAP

Then rescan and load **1.44** the same way (FX on a MIDI track).

## GUI map

- **LOAD** — browse for WAV/MP3 (no input-format cap; the cap is after Commit)
- **COMMIT** — resample/quantize the region to 16-bit at 44.1k or 29.76k; reject if over budget
- **44.1k / 29.76k** — hardware rate switch (live budget updates)
- **AUDITION / PREV LOOP** — preview the selected region
- **MONO** — downmix on commit
- **SNAP ZX** — snap loop markers to the nearest zero-crossing
- **ONE-SHOT / FORWARD / PINGPONG** — MIDI loop mode (no loop crossfade)
- Waveform: mouse wheel zoom, middle-drag pan, drag markers, click to scrub
- Budget slider: 64 KB–8 MB (default 1440 KB)
- ADSR + root note: note-off uses release so notes do not hard-cut

## Tests

Constraint math, MIDI playback-rate mapping, WAV/MP3 decode, and CLAP instantiate/process are required and run via `ctest`.

Byte-size formula (always 16-bit):

```
frames_out = round(region_frames * target_sr / source_sr)
bytes      = frames_out * channels * 2
```

`channels` is 1 with mono downmix, otherwise 2 for stereo sources. Commit is accepted only when `0 < bytes <= budget`.

Playback rate:

```
rate = 2^((midi_note - root_note) / 12)
```

## Layout

```
src/core/     constraint engine, decoder, voices, sampler
src/gui/      software-rendered editor + X11 embed
src/plugin/   CLAP entry, ports, params, state
tests/        unit tests + clap_probe host
third_party/  official CLAP SDK (vendored) + dr_wav/dr_mp3
```

## License

Plugin source is provided for this project. The vendored CLAP SDK is MIT (see `third_party/clap/LICENSE`). `dr_wav` / `dr_mp3` are public-domain / MIT-0 (mackron/dr_libs).
