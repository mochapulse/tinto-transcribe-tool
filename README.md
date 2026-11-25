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

One command, self-contained — downloads the `tinto` binary, sets up `~/.tinto`,
runs `tinto init`, and fetches the models your machine needs. No compiler or
toolchain required.

**Linux (x86_64)**

```sh
curl -fsSL https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/main/install/install.sh | bash
```

The installer detects your package manager (pacman, apt, dnf, zypper), installs
audio dependencies if missing, downloads the binary, runs `tinto init`, and
fetches the right models for your hardware. Use `--minimal` to skip model
download, `--system` to install to `/usr/local/bin`.

**Windows 11 (build from source)** — there is no installer yet. From an x64
Visual Studio developer shell with CMake,
Ninja, and Git on `PATH`, run:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
.\build\cli\tinto.exe init
.\build\cli\tinto.exe record --mode 1
.\build\cli\tinto.exe status
.\build\cli\tinto.exe stop
```

For local transcription, put an ASR model and the Silero VAD model in
`~/.tinto/models`, then choose the matching quality. For example, with
`ggml-base-q5_1.bin` and `ggml-silero-v5.1.2.bin` installed, run
`.\build\cli\tinto.exe record --mode 3 --quality base`.

## Usage

```sh
tinto init      # profile this machine, pick sensible defaults (run once)
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

On x64 Windows or Linux, enable the optional local speaker engine at configure
time with `-DTINTO_WITH_DIARIZATION=ON`. On Windows, keep the generated Sherpa
and ONNX Runtime DLLs next to `tinto.exe`.

Put the [Pyannote 3.0 int8 segmentation model](https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-segmentation-models/sherpa-onnx-pyannote-segmentation-3-0.tar.bz2)
at `~/.tinto/models/diarization/segmentation.int8.onnx` (the archive's
`model.int8.onnx`). Put [TitaNet Small](https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-recongition-models/nemo_en_titanet_small.onnx)
next to it as `embedding.onnx`.

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
- **[`AGENTS.md`](AGENTS.md)** — conventions, starting with the language rule
  (everything we build is English; we talk in Spanish).
