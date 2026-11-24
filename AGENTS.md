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

## What tinto is

`tinto` is a passive, local meeting recorder & transcriber: it captures audio,
keeps only speech (VAD gate), and optionally transcribes to Markdown. Pipeline:
**capture → DSP gate → mode dispatch**. CLI first; UI later.

**The full design and all decisions live in [`plans/`](plans/)** — read the
latest implementation plan there before changing architecture, and update it
when a decision changes.

## Key invariants (do not break; see the plan for rationale)

- **Capture is a passive sniffer, never an owner.** Shared-mode inputs (never
  WASAPI exclusive); outputs as a read-only copy (WASAPI loopback / sink
  monitor). No virtual cables, re-routing, or new default sinks.
- **One shared core.** CLI and the future UI reuse the same core; the CLI is the
  full feature set, not a subset. No per-interface forks.
- **Platform-agnostic.** Targets Windows 11, Debian-based, Arch-based Linux.
  OS-specific audio behind a clear boundary.
- **All runtime data under `~/.tinto/`** (override `$TINTO_HOME`). No audio in a
  database; audio is files, the live buffer is RAM.
- **Config precedence: CLI flag > machine profile (`~/.tinto/profile.json`) >
  built-in default.**
- **Local by default.** The Groq API backend sends audio off-machine — opt-in
  only, with a warning, never default.

## Working conventions

- **Language/runtime:** C/C++ core (Python only where C/C++ is impractical),
  built with CMake. Match the file's existing style.
- **Scope discipline:** make the change asked for. No speculative features,
  dependencies, or abstractions "for later."
- **Dependencies:** add one only when it earns its place; call it out, and mind
  that it must build on all three target platforms.
- **Tests before shortcuts:** favor correctness and maintainability over speed;
  add/update tests for behavior you change.
- **Secrets & data:** never commit audio, transcripts, API keys, or any captured
  content. Treat all captured audio as private.
- **Commits:** small, focused, imperative mood, English (e.g. `Add loopback
  capture for Windows 11`). Follow the repo's trailer convention (no AI
  attribution lines).
- **Versioning:** the repo-root `VERSION` file is the single source of truth.
  CMake reads it and generates `tinto/version.hpp` (do not edit a committed
  version header — there isn't one). Bump `VERSION` only, then tag the release
  to match (`vX.Y.Z`).

## Before you finish

- Re-read Rule 1 and confirm every file you wrote is in English.
- Verify the change runs / tests pass where applicable, and report honestly if
  something is skipped or failing.
- If a decision changed, update the plan in `plans/`.
