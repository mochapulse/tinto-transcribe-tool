# tinto — Implementation Plan

- **Created:** 2026-11-24 16:15 (America/Bogota, UTC−05)
- **Descriptor:** Full implementation plan and design decisions for the tinto CLI
  (passive meeting recorder & transcriber). CLI first; UI later.
- **Status:** Shared recording pipeline with foreground and background CLI
  control on Linux and Windows 11. Version 0.1 packages both platforms.

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
- **Follow the default device; handle hotplug** by periodically reconciling the
  enumerated endpoints on the pipeline thread. Open the default sink's own
  monitor on Linux. Recheck input transport before opening a new default mic.
- **Mix on the shared clock.** A bounded 30-second ring aligns input blocks
  against a monotonic sample timeline. A 150 ms holdback absorbs callback
  jitter. Missing channels contribute silence rather than extending the timeline.
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
  tiny chunks make RTF explode (~3×). Join nearby VAD regions on the original
  session timeline into windows of up to 30 s. Keep short pauses and flush
  sooner when VAD confirms a longer pause or recording ends. This window
  size, not the model alone, is the main real-time lever. Markdown uses
  Whisper's segment timestamps within each window, with millisecond precision.

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

- **Diarization (not identity):** use sherpa-onnx offline on CPU with
  Pyannote 3.0 int8 segmentation and TitaNet Small speaker embeddings. It is
  an opt-in x64 build dependency (`TINTO_WITH_DIARIZATION`). `record --mode 2
  --diarize` preserves the WAV/Markdown timeline by filling gaps with synthetic
  silence. The separate `diarize` command reads that WAV after recording and
  writes an annotated `.speakers.md` without changing the original transcript.
  Labels are anonymous and local to a recording. Assign labels by time overlap,
  showing multiple labels or `Unknown speaker` when appropriate. Mode 3 keeps
  no WAV. Live diarization is later work.
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
- **`record`** — start the pipeline in the selected mode and detach (fork +
  `setsid` on Linux, detached child process on Windows). Redirect stdio to the
  log and print the worker PID. Flags override
  the profile. `--foreground`/`-f` stays attached (debug/systemd).
- **`status`** — report the worker (mode, model, uptime, output, segments,
  CPU/threads, disk/RAM, device). Non-zero if none running.
- **`stop`** — graceful shutdown (flush, finalize `.md`/audio, cleanup, remove
  PID/socket). Hard kill only with `--force`.
- **`diarize`** — label speakers in a finished mode 2 recording prepared with
  `--diarize`. This runs independently from capture and Whisper.
- **Control plane:** an instance lock and a PID file under `~/.tinto/state/`.
  Linux requests shutdown with SIGTERM. Windows uses a local named pipe scoped
  to the instance and PID, then waits for the worker to finalize its files.
  Foreground recording also handles Ctrl+C (and Ctrl+Break on Windows).
  The Windows profiler uses the shared CPU model-selection rules.
- **Transcription shutdown:** stop accepting audio on the first request and
  give outstanding speech up to 25 seconds to finish. A second console stop
  signal cancels immediately at a backend checkpoint. Keep completed text,
  do not restart an interrupted batch, and mark a partial transcript. WAV
  finalization does not depend on ASR finishing.
  The pinned Whisper checkout is patched to forward its existing abort
  callback to the CPU backend during graph computation, not just after an
  encoder or decoder pass.
- One active worker per instance (named/multiple sessions = later work).
- **Config surface (flags):** `--mode`, `--quality {base|small|medium|turbo}`,
  `--source {input|system|both}`, `--device`, `--threads`/`--cpu-budget`,
  `--storage {ram|disk}`,
  `--format {wav|opus}`, `--retention`, `--window-sec`, `--lang`, `--out`,
  `--diarize`, `--word-timestamps`, `--backend {local|groq}`.

## 10. Deployment & install

The x64 Linux and Windows 11 installers are one-line entry points under
`install/`. Linux detects Debian or Arch families from `/etc/os-release`.
Windows checks the desktop build and x64 architecture. Each installs missing
runtime packages, verifies the SHA-256 of the release archive, and installs
the executable and its runtime libraries on the user’s PATH.

`tinto init` owns model provisioning on every platform. It verifies or fetches
all enabled Whisper, Silero, and diarization models using pinned file hashes.
`-n` skips that step when preparing an offline installation. `health` reports
setup and audio availability. `doctor` additionally verifies model checksums.

GitHub Actions builds and tests Windows and Linux on changes to `main` and
pull requests. The Linux package is also tested on Debian and Arch. A matching
version tag on `main` publishes the two verified archives and their checksums.
The Windows archive includes the ONNX Runtime and Sherpa DLLs. The Linux
archive keeps its shared libraries beside the application bundle.

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

On Windows 11 (native x64 MSVC builds launched from WSL via `/mnt/c/`):

- CTest passes with and without Whisper. `init` writes a Windows hardware
  profile and a detached worker that fails to start returns an error.
- Foreground WASAPI loopback produces a valid WAV from a synthetic tone and
  exits cleanly on Ctrl+Break. Silero and Whisper transcribe synthetic speech
  to Markdown in mode 3, then finalize on Ctrl+C without writing a WAV.
  `status` reports the active worker and the PID file is removed on exit.
- Windows opens the built-in microphone and WASAPI loopback simultaneously
  and delivers samples from both. The detached worker rejects a concurrent
  launch. `stop` finalizes WAV and Markdown files and removes its PID.
- The optional native `whisper-cancel` test exercises cancellation inside
  inference without opening an audio device.
- In a native Windows loopback test with three synthesized Latin American
  Spanish phrases, Medium recognized expected words from each phrase and
  finalized the Markdown on the first Ctrl+C without writing a WAV. A second
  Ctrl+C during a Turbo shutdown cancelled pending inference and kept a
  complete partial-transcript notice.
- The offline speaker engine processed a five-minute Spanish synthetic
  fixture in 28.60 seconds using two CPU threads (RTF 0.095). It found two
  clusters, matched all 40 isolated turns, and labeled all seven overlap
  intervals with both speakers. This is not a natural-meeting accuracy
  measurement.
- A native Windows mode 2 record/stop/diarize test preserved leading silence
  in the aligned WAV, labeled the latest session, and kept the original
  Markdown unchanged.

## 13. Remaining / open items

- Build toolchain specifics and how the (later) UI binds to the C/C++ core.
- Exact Spanish fine-tune to ship (evaluate on Colombian audio); model licenses.
- Storage watcher retention policy details.
- Validate capture and endpoint changes on Debian/Arch (Pulse monitor) and
  exercise Windows hotplug with physical devices.
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
