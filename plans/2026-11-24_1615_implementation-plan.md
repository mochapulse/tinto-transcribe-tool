# tinto — Implementation Plan

- **Created:** 2026-11-24 16:15 (America/Bogota, UTC−05)
- **Descriptor:** Full implementation plan and design decisions for the tinto CLI
  (passive meeting recorder & transcriber). CLI first; UI later.
- **Status:** Design complete and validated end-to-end on the dev box
  (Arch, Ryzen 7 7730U, CPU-only). Core not yet implemented.

This is the authoritative design record. `README.md` (users) and `AGENTS.md`
(contributor conventions) are intentionally short and point here for detail.

---

## 1. Goal

A lightweight **local** tool to store audio or transcribe it, as context fuel
for AI, note-taking, and a searchable record of what was said. It passively
captures audio, keeps only speech, and optionally transcribes to Markdown.

Pipeline: **capture → DSP speech gate → mode dispatch**.

```
 inputs + outputs            DSP speech
 (default mic +            classifier:                     Mode
  default sink monitor)    is someone talking?           dispatch
        │                        │                           │
        ▼                        ▼                           ▼
  ┌───────────┐  raw audio ┌──────────────┐  speech  ┌──────────────┐
  │  Capture  │ ─────────▶ │  Gate (VAD)  │ ───────▶ │   Pipeline   │
  └───────────┘            └──────────────┘          └──────────────┘
                                 │ noise / silence
                                 ▼  discard
```

## 2. Chosen stack (optimize, don't reinvent)

Core in **C/C++** (Python only where C/C++ is impractical); stdlib-only reserved
for hot paths where a dependency would cost performance. Build with **CMake**;
whisper.cpp linked as a **library** (not shelling out to `whisper-cli`).

| Layer | Base code | Notes |
| ----- | --------- | ----- |
| Audio capture | [miniaudio](https://miniaud.io/) | Single-header C/C++. WASAPI loopback on Windows; on Linux open the `.monitor` source as a capture device. |
| DSP speech gate | [whisper.cpp](https://github.com/ggml-org/whisper.cpp) + Silero VAD | VAD ships inside whisper.cpp (`--vad` + GGML VAD model). No separate VAD project. |
| Transcription | whisper.cpp | Local, GGML models. |
| Diarization (opt-in) | [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) | Segmentation + embedding + clustering, Apache-2.0, ONNX, offline, language-independent. |
| API fallback (opt-in) | [Groq](https://console.groq.com) | whisper-large-v3/turbo free tier, OpenAI-compatible. Sends audio off-machine. |

## 3. Capture design (passive, non-intrusive)

**Capture is a passive sniffer, never an owner.** Never take exclusive control
or change audio routing:

- **Inputs** open in **shared mode** (never WASAPI exclusive) so other apps keep
  working.
- **Outputs** are a read-only copy: WASAPI loopback on Windows; the sink
  **monitor source** on PulseAudio/PipeWire (Linux). Playback is never
  interrupted.
- **No virtual cables, no re-routing, no new/replaced default sinks.**

Best practices for the core:

- **Two logical streams, not N devices:** the default sink's monitor (what you
  hear) + the default mic (what you say). Extra devices are opt-in. One
  `ma_device` per source.
- **Synchronize to one clock:** each device drifts; resample every source to a
  common **16 kHz mono** stream and timestamp chunks with a monotonic clock.
- **Follow the default device; handle hotplug** via miniaudio
  `notificationCallback` (`default_device_changed`, `rerouted`). Never latch onto
  "the first monitor".
- **Bluetooth policy:** a BT output *monitor* is a clean passive tap. A BT *mic*
  is inherently intrusive (forces A2DP→HSP/HFP, degrades playback, can stall on
  first open) — opt-in with a warning, never default.
- **No PipeWire-native combined capture** (modifies routing + Linux-only).

## 4. VAD + transcription

- Prefer **SOTA open-weight** models running **locally**; free API is a fallback.
- Target **Latin American Spanish (Colombia)**; no open-weight model is
  specialized for this variant yet — start from a strong general Spanish model
  and validate on real Colombian audio.
- **Model depends on mode (real-time is compute-bound).** Benchmarks on the
  CPU-only dev box (Ryzen 7 7730U, `-t 8`), pure-speech RTF + quality on a real
  Spanish podcast clip:

  | Model | RTF (pure speech) | Quality |
  | ----- | ----------------- | ------- |
  | `large-v3-turbo-q5_0` | ~1.5 (30 s clip ~0.65) | best (reference) |
  | `medium-q5_0` | 1.08 | scrambled segment order in one test — rejected |
  | `small-q5_1` | 0.34 | near-turbo, one minor slip — **live pick** |
  | `base-q5_1` | 0.16 | lossy (hallucination, name errors) — fallback |

  **Default picks (CPU):** live modes (2, 3) → `small-q5_1`; batch/record mode
  (1, offline) → `large-v3-turbo`; `base` only for very weak CPUs. A usable GPU
  (CUDA) lets turbo go live — select per machine.
- **Threads = physical cores**, not logical (`-t 8` beat `-t 16`; fewer, worse).
- **VAD is also the #1 performance lever:** it skips non-speech, cutting the
  *effective* RTF roughly by the silence fraction (~50% on a podcast). It does
  **not** lower pure-speech RTF, so a slow model still lags on dense speech.
- **Live mode must buffer ~30 s windows, not small chunks.** whisper encodes in
  fixed 30 s windows at a per-window cost (turbo ≈ 15.7 s/window on this CPU), so
  tiny chunks make RTF explode (~3×). Stream with ~30 s windows + overlap. This
  window size, not the model alone, is the main real-time lever.

## 5. Modes

1. **Record only** — keep audio, no transcript.
2. **Record + transcribe** — keep audio and stream a real-time transcript to a
   `.md` file.
3. **Transcribe only** — real-time transcript, delete audio after processing.

## 6. Storage, CPU budget & data layout

- **No database for audio** (blobs belong in files). Working buffer = bounded
  **RAM ring buffer**.
  - Mode 3: audio stays in **RAM, never disk**, dropped after transcription.
  - Modes 1 & 2: **files on disk** — WAV 16 kHz mono (MVP); Opus option for long
    recordings, decoded to PCM only when transcribing.
  - Metadata (later): a light **SQLite** index (timestamps + audio/transcript
    paths) for search — pointers only, never audio. A vector index over
    transcripts is a possible future feature.
- **All data under a single `~/.tinto/`** (override `$TINTO_HOME`):

  ```
  ~/.tinto/
  ├── profile.json     # machine profile from `init`
  ├── config.toml      # user config (optional; flags override)
  ├── models/          # GGML model cache (ASR + VAD)
  ├── state/           # per-instance PID file + control socket
  ├── logs/            # background worker logs
  ├── recordings/      # persisted audio (modes 1/2)
  └── transcripts/     # .md outputs           (--out overrides)
  ```
- **CPU budget leaves room for other work.** Heavy consumer is whisper encode;
  capture/VAD are trivial. `--threads` configurable, `--cpu-budget %` shortcut.
  Defaults: live = physical cores − 2 (small keeps RTF margin), batch = all
  physical. Transcription thread at low priority; capture on its own untouchable
  real-time thread.

## 7. Speaker differentiation, timestamps & API fallback

- **Diarization (not identity):** tell voices apart as anonymous `Speaker 1/2/…`
  via sherpa-onnx (language-independent). Opt-in (`--diarize`), fits batch/record;
  online/live diarization is later work. (`tinydiarize` rejected: English-only.)
- **Timestamps:** segment-level on by default; word-level opt-in
  (`--word-timestamps`) via whisper.cpp DTW (`--dtw <preset>`). Preset must match
  the model; `small` has one (synergy with the live pick), turbo/large-v3 may
  not. ~tens of ms accuracy — good for attribution/seek, not forensic.
- **Free API fallback (opt-in only):** Groq free tier behind the same interface
  (`--backend groq`). **Sends audio off-machine → breaks the local/privacy
  stance: never default, explicit opt-in + warning, needs network + API key.**
  Suits weak CPUs or top-quality-without-local-compute; rate limits suit batch.

## 8. Zero-flag defaults (install-time hardware profile)

- `tinto init` profiles the machine and writes `~/.tinto/profile.json` so a
  casual user runs with **no flags**.
- **Precedence: CLI flag > machine profile > built-in default.**
- Detects OS/distro, CPU, physical vs logical cores, RAM, AVX2, GPU/accel.
- Heuristic: accel → turbo live+batch; CPU ≥8 cores → small live / turbo batch;
  4–7 → base live / small batch; <4 → base (+ suggest `--backend groq`); RAM
  <3 GB caps at base; `threads_live = cores−2`, `threads_batch = cores`.
- `init --calibrate` runs the RTF benchmark and overwrites model picks with
  measured numbers (core count alone mis-predicts RTF).
- Prototype: `examples/hardware-profiler/arch-09-profile.sh`.

## 9. CLI commands & daemon model

Daemon-oriented: `record` starts a background worker and returns the terminal.

- **`init`** — profile machine, write defaults. `--calibrate` for benchmarked.
- **`record`** — start the pipeline in the selected mode; **detaches** (fork +
  `setsid`, stdio → log file) and returns, printing PID/handle. Flags override
  the profile. `--foreground`/`-f` stays attached (debug/systemd).
- **`status`** — report the worker (mode, model, uptime, output, segments,
  CPU/threads, disk/RAM, device). Non-zero if none running.
- **`stop`** — graceful shutdown (flush, finalize `.md`/audio, cleanup, remove
  PID/socket). Hard kill only with `--force`.
- **Control plane:** per-instance PID file + control socket under
  `~/.tinto/state/`. Unix domain socket on Linux/macOS, named pipe on Windows,
  behind one abstraction.
- One active worker per instance (named/multiple sessions = later work).
- **Config surface (flags):** `--mode`, `--quality {base|small|medium|turbo}`,
  `--device`, `--threads`/`--cpu-budget`, `--storage {ram|disk}`,
  `--format {wav|opus}`, `--retention`, `--window-sec`, `--lang`, `--out`,
  `--diarize`, `--word-timestamps`, `--backend {local|groq}`.

## 10. Deployment & install

One-line, self-contained, **CLI only for now** (UI later).

- Linux/macOS: `curl -fsSL .../install/install.sh | sh`
- Windows (PowerShell): `irm .../install/install.ps1 | iex`

Installer: detect OS+arch → download prebuilt **self-contained** binary
(whisper.cpp + miniaudio statically linked, no toolchain) from GitHub Releases →
install to PATH → create `~/.tinto/` → run `tinto init` → download
profile-selected models (ASR+VAD) so first run works offline (`--minimal` skips)
→ verify + print quickstart.

- **Self-contained:** one binary per platform (linux x86_64/arm64, windows
  x86_64); no runtime deps beyond the OS audio stack (PulseAudio/PipeWire,
  WASAPI). Diarization (sherpa-onnx/ONNX Runtime) is a separate add-on.
- **Release pipeline:** GitHub Actions cross-builds + publishes binaries and a
  models manifest to GitHub Releases (pinned/reproducible). Scripts in `install/`.
- **Uninstall:** remove the binary and `~/.tinto`.

## 11. Platforms

Platform-agnostic, compiled per target: **Windows 11**, **Debian-based** Linux,
**Arch-based** Linux. Isolate OS-specific audio code behind a clear boundary.

## 12. Validation log (what is already proven)

On the dev box (Arch, PipeWire 1.6.9 + PulseAudio compat, Ryzen 7 7730U, CPU-only):

- **Passive capture** (`examples/pipeline-mic-audio`): device enumeration finds
  monitor sources; shared-mode mic capture works (RMS verified); loopback via the
  sink monitor works (played a 440 Hz tone, captured RMS 0.08); simultaneous
  multi-mic capture works. BT mic caveat confirmed.
- **Transcription + VAD** (`examples/RTF-VAD`): whisper.cpp builds, Silero VAD
  works, real Spanish podcast transcribed cleanly with turbo; RTF benchmarked
  across base/small/medium/turbo; model decision validated.
- **Hardware profiler** (`examples/hardware-profiler`): picks live=small/6,
  batch=turbo/8 on this box — matches the benchmarked decision.

## 13. Remaining / open items

- Build toolchain specifics and how the (later) UI binds to the C/C++ core.
- Exact Spanish fine-tune to ship (evaluate on Colombian audio); model licenses.
- Storage watcher retention policy details.
- Capture on Windows 11 and Debian (no machines available yet; pattern is
  standard WASAPI loopback / Pulse monitor).
- miniaudio Linux monitor-source selection following the current default sink.

## 14. Milestones

1. **Scaffold:** CMake project, `core/` library + `cli/`, whisper.cpp + miniaudio
   linked; `tinto` with `init`/`record`/`status`/`stop` stubs; config precedence
   (flag > profile > default); `~/.tinto` layout.
2. **Vertical slice (Linux, CLI):** one stream (default sink monitor) → VAD →
   `small` → mode 3 (live transcript to `.md`, ~30 s windows). Daemonized
   `record` + `status`/`stop` over the control socket.
3. Second stream (mic) + clock sync; modes 1 and 2; storage watcher.
4. `install/` scripts + GitHub Actions release pipeline (Linux first).
5. Windows 11 + Debian capture; packaging per platform.
6. Opt-in diarization and word-level timestamps; Groq backend.
7. UI over the shared core.
