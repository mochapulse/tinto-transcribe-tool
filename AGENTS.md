# AGENTS.md

Conventions for AI coding agents (and humans) working in this repository.

## Rule 1 — Language: write in English, talk in Spanish

This is the top rule, and it is not optional.

- **All artifacts are in English.** Code, identifiers, comments, docstrings,
  commit messages, PR descriptions, documentation, UI copy, logs, and tests are
  written in English, using neutral/professional wording.
- **Conversation happens in Spanish.** Chat, explanations, questions, and
  discussion with the maintainer are in Spanish (neutral, professional — no
  regional slang).

In short: **everything we build is English; the way we talk about it is
Spanish.**

## Project intent (context for your changes)

tinto captures audio from every input and output device,
uses a DSP classifier to keep only segments that contain speech (discarding
noise and silence), and optionally transcribes them in real time. The goal is a
lightweight local tool to store or transcribe spoken audio — useful for AI
context extraction, notes, and similar. See `README.md` for the full picture.

Keep changes aligned with the pipeline: **capture → DSP gate → mode dispatch**.

## Architecture rules

- **One shared core.** The CLI and the UI must reuse the same backend/core
  code. Do not fork logic per interface — capture, gating, and transcription
  live in one place with a single source of truth.
- **The CLI is the full feature set.** Any capability must be reachable from the
  CLI; the UI is a frontend over the same core, not a superset.
- **Platform-agnostic, compiled for performance.** Targets are Windows 11,
  Debian-based, and Arch-based Linux. Avoid platform-locked APIs in the core;
  isolate OS-specific audio code behind a clear boundary. Favor designs that
  compile to efficient native builds on each target.
- **DSP gate is mandatory.** Audio only advances if the speech classifier says
  someone is talking. Noise and silence are discarded. A storage watcher must
  continuously delete old audio so the archive stays bounded.
- **Capture is a passive sniffer, never an owner.** Do not take exclusive
  control of any device or change the system's audio routing:
  - Open inputs in **shared mode** (never WASAPI exclusive) so other apps keep
    working.
  - Capture outputs as a read-only copy: WASAPI loopback on Windows, the sink
    **monitor source** on PulseAudio/PipeWire (Linux). Never interrupt playback.
  - No virtual audio cables, no re-routing, no creating/replacing default sinks.

## Capture best practices (core)

Validated on real hardware with the `examples/arch-*` probes (miniaudio +
PipeWire). These apply to the core, not the probes.

- **Two logical streams, not N devices.** The default capture target is exactly
  two streams: the **default output sink's monitor** (what you hear from
  everyone) and the **default microphone** (what you say). Do not blindly open
  every device — extra inputs/outputs are **opt-in**. One `ma_device` per source.
- **Synchronize to one clock.** Each device runs on its own clock and drifts
  over a long meeting. Pick a master clock, **resample every source to a common
  16 kHz mono** stream, and timestamp chunks with a monotonic clock so streams
  can be aligned. miniaudio does not sync this for us.
- **Follow the default device; handle hotplug.** Never latch onto "the first
  monitor". Use miniaudio's `notificationCallback`
  (`default_device_changed`, `rerouted`) to re-open when the default sink/source
  changes or devices appear/disappear (e.g. switching to Bluetooth).
- **Bluetooth policy.** A BT **output monitor** is a clean passive tap — use it.
  A BT **microphone** is inherently intrusive: capturing it forces an
  A2DP→HSP/HFP profile switch that degrades playback to telephone quality and
  can stall on first open. Treat BT mic capture as **opt-in with a warning**,
  never a default.
- **Do not reach for PipeWire-native combined capture.** Creating a mixing
  node/filter-chain to grab one combined stream modifies routing (violates the
  passive rule) and is Linux-only (breaks portability). Use the portable
  per-source + monitor-tap model plus our own clock sync instead.

## Modes

Three initial modes must be respected when touching the pipeline:

1. Record audio only.
2. Record audio **and** stream a real-time transcript to a `.md` file.
3. Real-time transcript only, deleting audio after it is processed.

## CLI commands & daemon model

The CLI is **daemon-oriented**: `record` starts a background worker and returns
the terminal immediately; the other commands control it.

- **`init`** — profile the machine and write the defaults profile (see
  "Zero-flag defaults"). Runs at global install, re-runnable. `--calibrate` for
  the benchmarked variant.
- **`record`** — start capture → gate → (transcribe) in the selected mode. It
  **detaches from the terminal** (daemonizes: fork + `setsid`, stdio redirected
  to a log file) and returns at once, printing the PID/handle. Honors all
  config flags (which override the machine profile). `--foreground`/`-f` opts out
  of detaching, for debugging or running under a service manager (systemd).
- **`status`** — report the running worker: mode, model, uptime, current output
  file, segments transcribed, CPU/threads, disk/RAM used, active device. Exit
  non-zero if nothing is running.
- **`stop`** — signal the worker to shut down **gracefully**: flush the buffer,
  finalize the `.md`/audio outputs, clean up, remove the PID/socket. Never a hard
  kill unless `--force`.
- **Control plane:** a per-instance **PID file + control socket** under
  `~/.tinto/state/` (see the data layout). Unix domain socket on Linux/macOS,
  named pipe on Windows — behind one abstraction, per the platform-agnostic
  rule. `status`/`stop` talk to the worker over it.
- **One active worker** per instance; a second `record` while one runs fails with
  a clear message (named/multiple sessions are later work). The PID file guards
  against double-start and is cleaned on exit.

## Transcription rules

- Prefer **state-of-the-art, foundational, open-weight** models running
  **locally**. A **free API** is an acceptable fallback, not the default.
- Target **Latin American Spanish (Colombia)**. No open-weight model is
  specialized for this variant yet, so start from a strong general Spanish model
  and validate against real Colombian audio before claiming it works.
- **Model depends on the mode, because real-time is compute-bound.** Benchmarked
  with whisper.cpp on the CPU-only dev box (Ryzen 7 7730U, `-t 8`). Pure-speech
  RTF (no VAD, dense audio) and quality on a real Spanish podcast clip:
  - `large-v3-turbo-q5_0`: RTF 1.5 — best quality (reference).
  - `medium-q5_0`: RTF 1.08 — rejected (not live-viable, no quality win worth it).
  - `small-q5_1`: RTF 0.34 — near-turbo quality (one minor error, got names right).
  - `base-q5_1`: RTF 0.16 — lossy (hallucinated a word, "papás"→"pocos").
  - **Default picks (CPU):** live modes (2, 3) → **`small-q5_1`**; batch/record
    mode (1, offline) → **`large-v3-turbo`**; `base` only as a fallback for very
    weak CPUs (expect quality loss). A usable GPU (CUDA) lets turbo go live;
    select per machine.
- **Thread count = physical cores**, not logical threads. On the 8-core/16-thread
  dev box, `-t 8` beat `-t 16`; fewer than 8 was worse.
- **VAD (Silero, built into whisper.cpp) is also the #1 performance lever.** It
  skips non-speech, cutting the *effective* RTF roughly in proportion to how much
  of the audio is silence/music (≈50% on a real podcast). It does not lower the
  pure-speech RTF, so a model that fails pure-speech RTF still lags during dense
  continuous speech — do not treat VAD as a substitute for picking a fast model.
- **Live mode must buffer large (~30 s) windows, not small chunks.** whisper
  encodes in fixed 30 s windows; the encoder cost is per-window regardless of how
  much audio the window holds (turbo ≈ 15.7 s/window on the dev CPU). So a 30 s
  clip gives RTF ≈ 0.65 for turbo, but chopping live audio into e.g. 5 s chunks
  makes each chunk pay most of that window cost and RTF explodes (~3×). Stream
  with ~30 s windows plus overlap (whisper.cpp's `stream` sliding approach), not
  tiny chunks. This window size, not the model alone, is the main real-time lever.
  (Earlier single-number RTFs from the 11 s `jfk.wav` were inflated by padding.)

## Storage, CPU & configuration

- **Everything important is configurable from the CLI** (flag overrides config
  file): `--mode`, `--quality {base|small|medium|turbo}`, `--device`,
  `--threads`/`--cpu-budget`, `--storage {ram|disk}`, `--format {wav|opus}`,
  `--retention`, `--window-sec`, `--lang`, `--out`, `--diarize`,
  `--word-timestamps`, `--backend {local|groq}`.
- **All tinto data lives under a single `~/.tinto/`** (override with `$TINTO_HOME`),
  not scattered across XDG dirs. Layout:
  - `~/.tinto/profile.json` — machine profile from `init`.
  - `~/.tinto/config.toml` — user config (optional; flags override it).
  - `~/.tinto/models/` — downloaded GGML model cache (ASR + VAD).
  - `~/.tinto/state/` — per-instance PID file + control socket.
  - `~/.tinto/logs/` — background worker logs.
  - `~/.tinto/recordings/`, `~/.tinto/transcripts/` — default output for persisted
    audio and `.md` transcripts (a `--out` path overrides).
- **No database for audio.** Audio is a large binary blob; databases handle those
  badly. Audio lives in **files**; the working buffer lives in a **bounded RAM
  ring buffer** (that is processing, not storage).
  - Mode 3 (transcribe-only): audio stays in **RAM, never touches disk**, dropped
    after transcription (privacy, no disk wear).
  - Modes 1 & 2 (keep audio): **files on disk** — WAV 16 kHz mono for the MVP
    (~1.9 MB/min, already validated); **Opus** as an option for long recordings
    (~4× smaller), decoded to PCM only when transcribing.
  - Metadata (later): a light **SQLite** index of timestamps + audio path +
    transcript path, for searching transcripts. Never store audio in it — only
    pointers. A vector index over transcripts is a possible future feature, not
    audio storage.
- **CPU budget leaves room for other work.** The heavy consumer is whisper's
  encode; capture and VAD are trivial. `--threads` is configurable, `--cpu-budget
  %` is a friendly shortcut. Defaults: **live = physical cores − 2** (small keeps
  comfortable RTF margin, so spending less CPU is free), **batch = all physical
  cores**. The transcription thread runs at **low priority** so secondary tasks
  preempt it; capture always runs on its own untouchable real-time thread.

## Zero-flag defaults: install-time hardware profile

- **`init` runs at global install and profiles the machine**, then writes the
  machine profile to `~/.tinto/profile.json` (see the data layout). The goal: a
  casual user runs the CLI with **no flags** and gets sensible behavior for their
  hardware.
- **Runtime precedence: CLI flag > machine profile > built-in default.** Flags
  always win; the profile just moves the defaults closer to the machine.
- **What it detects:** OS/distro, CPU model, physical cores vs logical threads,
  RAM, AVX2, GPU and accel (cuda/rocm/cpu).
- **How it picks defaults (heuristic):** accel present → turbo live+batch, all
  threads. CPU-only: `threads_live = cores−2`, `threads_batch = cores`; model by
  core count (≥8 → small live / turbo batch; 4–7 → base live / small batch; <4 →
  base, suggest `--backend groq`); low RAM (<3 GB) caps at base.
- **`init --calibrate` (more robust than the heuristic):** run a short RTF
  benchmark (see `examples/RTF-VAD`) and overwrite the model picks with measured
  numbers — because core count alone mis-predicts RTF (whisper's 30 s-window cost).
- Prototype: `examples/hardware-profiler/arch-09-profile.sh`.

## Speaker differentiation, timestamps & API fallback

- **Speaker differentiation (diarization, not identity).** Goal: tell voices
  apart (anonymous `Speaker 1/2/…`) to enrich context, never identify people.
  Use **[sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)** diarization
  (segmentation + speaker-embedding + clustering, Apache-2.0, ONNX, offline,
  C/C++). It works on voice characteristics, so it is **language-independent**
  (fine for Spanish). Rejected: whisper.cpp `tinydiarize` (English-only,
  prototype). Diarization is an **opt-in** stage (`--diarize`, off by default —
  extra model + CPU); it fits **batch/record** modes well. Online (live)
  diarization is harder and may add latency — treat as later work.
- **Precise timestamps.** Segment timestamps are on by default (whisper emits
  them). **Word-level timestamps are opt-in** (`--word-timestamps`) via
  whisper.cpp DTW (`--dtw <preset>`, cross-attention + dynamic time warping). The
  preset must match the model; `small` has a preset (synergy with the live pick),
  whereas turbo/large-v3 may lack one — another reason word-level timing rides on
  `small`. Accuracy is ~tens of ms and model-dependent: good for speaker
  attribution and seeking, not forensic. Diarization needs these to label lines.
- **Free API fallback (opt-in only).** **[Groq](https://console.groq.com)** runs
  whisper-large-v3 / turbo on a free tier (~20 req/min, ~2000/day, ~7200 audio
  s/hour, 25 MB/request) — a drop-in alternative transcription backend behind the
  same interface (`--backend groq`), sending gated speech segments to its
  OpenAI-compatible `/audio/transcriptions`. **It sends audio off the machine, so
  it breaks the local/privacy stance: never a default, always explicit opt-in
  with a clear warning, needs network + API key.** Best for weak CPU-only
  machines or when large-v3 quality is wanted without local compute; rate limits
  suit batch, not 24/7 live.

## Chosen stack

Optimize, but reuse mature code — do not reinvent the wheel. Core in **C/C++**;
Python only where C/C++ is impractical. Stdlib-only is reserved for hot paths
where a dependency would cost performance.

- **Audio capture:** [miniaudio](https://miniaud.io/) (single-header C/C++).
  Windows loopback is native; on Linux, open the `.monitor` source as a capture
  device.
- **DSP speech gate + transcription:** [whisper.cpp](https://github.com/ggml-org/whisper.cpp).
  Silero VAD is built in (`--vad` + a GGML VAD model) — no separate VAD project.
- **Model:** Whisper **large-v3-turbo** as the starting point; evaluate Spanish
  fine-tunes and convert the winner to GGML.

Still to verify before relying on them: miniaudio's Linux monitor-source path,
and the licenses of any model/fine-tune we ship.

## Working conventions

- **Scope discipline:** make the change that was asked for. No speculative
  features, dependencies, or abstractions "for later."
- **Dependencies:** add one only when it earns its place; call it out
  explicitly. Mind that it must build on all three target platforms.
- **Tests before shortcuts:** favor correctness and maintainability over speed.
  Add or update tests for behavior you change.
- **Secrets & data:** never commit audio recordings, transcripts, API keys, or
  any captured content. Treat all captured audio as private.
- **Commits:** small, focused, imperative mood, English (e.g. `Add loopback
  capture for Windows 11`).

## Open design decisions

Not settled yet — raise them for discussion (in Spanish) rather than deciding
silently:

- The exact Spanish fine-tune to ship (after evaluation on Colombian audio).
- The build toolchain and how the UI binds to the C/C++ core.
- The on-disk format and retention policy of the storage watcher.

## Before you finish

- Re-read Rule 1 and confirm every file you wrote is in English.
- Verify the change runs / tests pass where applicable, and report honestly if
  something is skipped or failing.
