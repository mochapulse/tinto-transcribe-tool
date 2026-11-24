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

**Linux / macOS**

```sh
curl -fsSL https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/main/install/install.sh | sh
```

**Windows (PowerShell)**

```powershell
irm https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/main/install/install.ps1 | iex
```

> Early stage — release binaries are not published yet.

## Usage

```sh
tinto init      # profile this machine, pick sensible defaults (run once)
tinto record    # start recording/transcribing in the background, frees the terminal
tinto status    # what the background worker is doing
tinto stop      # finalize outputs and stop
```

`record` works with no flags using the profile from `init`; any flag
(`--mode`, `--quality`, `--diarize`, …) overrides it. `record -f` stays in the
foreground. All data lives under `~/.tinto/`.

**CLI only for now; a UI will come later, reusing the same core.**

## Platforms

Windows 11, Debian-based Linux, Arch-based Linux — one self-contained binary per
platform.

## Status

Early stage. The design is complete and validated end-to-end (see
[`plans/`](plans/)); the core is not yet implemented. The `examples/` directory
holds the hardware-validation probes that back the design.

## Contributing

- **[`plans/`](plans/)** — the full implementation plan and design decisions.
- **[`AGENTS.md`](AGENTS.md)** — conventions, starting with the language rule
  (everything we build is English; we talk in Spanish).
