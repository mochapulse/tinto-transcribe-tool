# tinto

> **T**ranscribe **IN**, **T**ext **O**ut — a passive, local meeting recorder
> and transcriber. (*tinto*: black coffee, Colombian style.)

Capture audio on your own machine, keep only what actually contains speech, and
optionally turn it into real-time Markdown transcripts. Built as context fuel
for AI, note-taking, and a searchable record of what was said.

## How it works

```
 default mic + sink monitor     VAD gate:                    mode
        │                   is someone talking?            dispatch
        ▼                        ▼                            ▼
  ┌───────────┐  audio   ┌──────────────┐  speech   ┌──────────────┐
  │  Capture  │ ───────▶ │  Gate (VAD)  │ ────────▶ │   Pipeline   │
  └───────────┘          └──────────────┘           └──────────────┘
                               │ noise / silence → discard
```

1. **Capture** — passively, two streams by default: the system output (what you
   hear) and your microphone (what you say). It never takes over your audio
   devices or interrupts playback.
2. **Gate** — a voice-activity detector keeps speech and discards silence/noise;
   old audio is cleaned up automatically.
3. **Pipeline** — process the kept audio per the selected mode.

## Modes

| Mode | Keeps audio | Real-time transcript | Deletes audio after |
| ---- | :---------: | :------------------: | :-----------------: |
| **1 — Record only**         | ✅ | — | — |
| **2 — Record + transcribe** | ✅ | ✅ (to a `.md`) | — |
| **3 — Transcribe only**     | — | ✅ | ✅ |

Transcription runs **locally**, tuned for **Latin American Spanish (Colombia)**.
The model is chosen per machine (fast model for live, best model for offline).

## Install

The installers detect the platform, install missing system prerequisites, verify
the GitHub Release archive, and run `tinto init`. No compiler is needed.

**Debian-based or Arch-based Linux (x86_64)**

```sh
curl -fsSL https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/main/install/install.sh | bash
```

**Windows 11 (x64 PowerShell)**

```powershell
irm https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/main/install/install.ps1 | iex
```

Initial setup downloads about 1.4 GB for the four Whisper qualities, Silero VAD,
and both speaker-labeling models. Existing files are verified before reuse.
Use `tinto health` for a quick setup check and `tinto doctor` for details.
Use the installer’s `-n` option or `tinto init -n` to skip model downloads.

<details>
<summary>Build from source</summary>

From a developer shell with CMake, a C++ compiler, Ninja, and Git:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DTINTO_WITH_DIARIZATION=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix stage --component TintoRuntime
```

The staged directory contains the executable and any required runtime libraries.
Run its `tinto init` to prepare the local profile and models. Omit the
diarization option for a build without speaker labeling.

</details>

## Usage

```sh
tinto init      # write the profile and download all models
tinto health    # check the setup
tinto doctor    # verify checksums and show diagnostics
tinto help record
tinto record    # start recording/transcribing in the background, frees the terminal
tinto status    # what the background worker is doing
tinto stop      # finalize outputs and stop
```

`record` works with no flags using the profile from `init`; any flag
(`--mode`, `--quality`, `--diarize`, …) overrides it. `record -f` stays in the
foreground and stops on Ctrl+C. All data lives under `~/.tinto/`.
Whisper groups nearby speech into windows of up to 30 seconds to keep context.
Ctrl+C stops accepting audio and allows up to 25 seconds for pending text.
Press Ctrl+C again to cancel the remaining work. The transcript keeps completed
text and notes if audio was left untranscribed.

By default, `record` mixes the default output and a non-Bluetooth microphone
on a shared timeline. On Windows it reads the output via WASAPI loopback. On
Linux it reads the default PulseAudio/PipeWire sink monitor. If a default
source is unavailable or the input transport is unverified, the other source
can still record. To override the defaults:

- `--source system` captures only the system output.
- `--source input` captures only the default safe input.
- `--source both` requires both sources at startup.
- `--device NAME` selects an input by name. Selecting a Bluetooth microphone
  can lower playback quality. It records only that input unless combined with
  `--source both`.

**CLI only for now; a UI will come later, reusing the same core.**

## Speaker labels after recording

The release builds include the optional local speaker engine. For source builds,
enable `-DTINTO_WITH_DIARIZATION=ON`. `tinto init` installs both speaker
models whenever the engine is enabled.

```sh
tinto record --mode 2 --diarize
tinto stop
tinto diarize
```

The recording option keeps an aligned WAV: gaps are written as silence so WAV
and Markdown timestamps match. No speaker analysis runs during capture. After
`stop`, `diarize` labels the latest eligible transcript and creates a separate
`.speakers.md` file. The original text and WAV remain unchanged. Mode 3 does
not keep audio and cannot be labeled afterward.

Labels such as `Speaker 1` apply only to that recording. A text segment can
show two labels when it spans both voices, or `Unknown speaker` when the
turns do not match. Use `tinto diarize --speakers 2` if you know the count, or
`tinto diarize --help` to select a specific session or output path.

## Platforms

Windows 11, Debian-based Linux, Arch-based Linux — one self-contained binary per
platform.

## Status

The shared CLI pipeline runs in the foreground or as a background worker on
Windows and Linux. Windows `stop` waits for the worker to finalize its outputs.
See [`plans/`](plans/) for the full design and [`AGENTS.md`](AGENTS.md) for
conventions. The `examples/` directory holds hardware-validation probes.

## Contributing

- **[`plans/`](plans/)** — the full implementation plan and design decisions.
- **[Platform handoff](docs/platform-handoff.md)** — verified Windows state, CPU-only Arch setup, and links to the archived benchmarks.
- **[`AGENTS.md`](AGENTS.md)** — conventions, starting with the language rule
  (everything we build is English; we talk in Spanish).
