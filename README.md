# MochaMeetRecorderTranscriber

Capture audio from any device, keep only what actually contains speech, and
optionally turn it into real-time transcripts. Built as context fuel for AI,
note-taking, and anything else that benefits from a searchable record of what
was said.

## What this project is for

A lightweight tool to **store audio or transcribe it**, running locally on your
own machine. Typical uses:

- Extracting context from meetings/calls to feed into AI workflows.
- Automatic notes from conversations.
- Keeping a lean archive of spoken content without hoarding useless noise.

## How it works

```
 All inputs + outputs            DSP speech
 (mics, system audio,          classifier:                     Mode
  loopback, every device)      is someone talking?           dispatch
        │                            │                           │
        ▼                            ▼                           ▼
  ┌───────────┐   raw audio   ┌──────────────┐   speech   ┌──────────────┐
  │  Capture  │ ────────────▶ │  Gate (DSP)  │ ─────────▶ │   Pipeline   │
  └───────────┘               └──────────────┘            └──────────────┘
                                     │ noise / silence
                                     ▼
                                  discard
```

1. **Capture** — by default, two logical streams: the **default output sink's
   monitor** (what you hear from everyone) and the **default microphone** (what
   you say). Additional inputs/outputs are **opt-in**. All sources are resampled
   to a common 16 kHz mono clock and timestamped so they stay in sync.
2. **DSP gate** — a digital-signal-processing classifier decides whether the
   audio contains speech. If **yes**, the segment is kept for processing; if it
   is **noise or silence**, it is discarded. A storage watcher continuously
   removes old audio so the archive never grows unbounded.
3. **Pipeline** — process the kept audio according to the selected mode.

### Capture must be passive (a sniffer, never an owner)

The tool **must not take ownership of audio devices** or alter the machine's
existing audio routing. It is a passive tap:

- **Inputs** are opened in **shared mode** so other apps keep using the same
  microphone at the same time. Exclusive mode is forbidden.
- **Outputs** are captured as a read-only copy of what is already playing —
  WASAPI loopback on Windows, and the sink **monitor source** on
  PulseAudio/PipeWire (Linux). Playback is never interrupted or modified.
- **No virtual audio cables, no re-routing, no new default sinks.** We read the
  existing streams; we do not reconfigure the user's audio.

## Modes

Three initial modes:

| Mode | Keeps audio | Real-time transcript | Deletes audio after processing |
| ---- | :---------: | :------------------: | :----------------------------: |
| **1 — Record only**            | ✅ | — | — |
| **2 — Record + transcribe**    | ✅ | ✅ (streamed to a `.md` file) | — |
| **3 — Transcribe only**        | — | ✅ | ✅ |

## Transcription

- **State-of-the-art, foundational, open-weight models** run **locally** by
  preference; a **free API** is an acceptable fallback.
- **Target language: Latin American Spanish (Colombia).** No open-weight model
  is specialized for this variant yet, so we start from a strong general Spanish
  model and validate it against our own Colombian audio.
- **Model is chosen per mode and per machine.** Real-time transcription is
  compute-bound: on a CPU-only laptop, `large-v3-turbo` runs slower than real
  time (measured RTF ≈ 1.5), so **live** modes use a smaller model that keeps
  RTF < 1, while **batch/record** modes use the best-quality model. A GPU lifts
  this limit. See `examples/RTF-VAD/`.

## Technology (chosen base)

We optimize, but we do not reinvent the wheel. The core is built in **C/C++**
(Python only as a fallback where C/C++ is impractical), reusing mature
libraries. Stdlib-only is reserved for hot paths where a dependency would cost
performance.

| Layer | Base code | Notes |
| ----- | --------- | ----- |
| Audio capture | **[miniaudio](https://miniaud.io/)** | Single-header C/C++, cross-platform. WASAPI loopback on Windows; on Linux, open the `.monitor` source as a capture device. |
| DSP speech gate | **[whisper.cpp](https://github.com/ggml-org/whisper.cpp) + Silero VAD** | VAD ships inside whisper.cpp (`--vad` + a GGML VAD model). No separate VAD project needed. |
| Transcription | **whisper.cpp** | Local, GGML models; built-in streaming example. |
| Model | **Whisper large-v3-turbo** | Starting point; Spanish fine-tunes to be evaluated and converted to GGML. |

Glue code we own: device management, the storage watcher, the three-mode
dispatch, and the CLI/UI over the shared core.

## Platforms

Platform-agnostic, targeting:

- **Windows 11**
- **Debian-based** Linux
- **Arch-based** Linux

Builds are **compiled for maximum performance** on each target.

## Interfaces

A **CLI** exposing the full feature set, daemon-oriented so `record` frees your
terminal:

```sh
mochameet init      # profile this machine, pick sensible defaults (run once)
mochameet record    # start recording/transcribing in the background, return the terminal
mochameet status    # what the background worker is doing
mochameet stop      # finalize outputs and stop
```

`record` runs with no flags using the machine profile from `init`; any flag
(`--mode`, `--quality`, `--diarize`, …) overrides it. `record -f` stays in the
foreground (for debugging or systemd).

A **UI** will **reuse the same core** — backend and frontend share one source of
truth for capture, gating, and transcription.

## Status

Early stage. The repository currently contains only project scaffolding. The
sections above describe the **intended** design, not finished functionality.

## Contributing

Read `AGENTS.md` before making changes — it defines the project's conventions,
starting with its first and most important rule about language.
