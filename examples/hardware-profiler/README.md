# hardware-profiler — prototype of the CLI `init` step

Explores the machine, decides per-machine defaults, and writes a profile so
casual users never have to pass flags. This is the prototype of what the real
CLI runs once at global install (`tinto init`).

Runtime precedence: **CLI flag > machine profile > built-in default**.

## Run

```sh
./arch-09-profile.sh
# writes $TINTO_PROFILE (default ~/.config/tinto/profile.json)
TINTO_PROFILE=/tmp/profile.json ./arch-09-profile.sh   # custom path
```

## What it does

1. Detects OS/distro, CPU, physical cores vs logical threads, RAM, AVX2, GPU/accel.
2. Picks defaults (model + threads per mode, window, storage, lang, backend).
3. Writes a JSON profile and prints the reasoning.

## Heuristic (decision table)

| Condition | live model | batch model | threads (live/batch) |
| --------- | ---------- | ----------- | -------------------- |
| GPU (cuda/rocm) | turbo | turbo | all / all |
| CPU, ≥8 cores | small | turbo | cores−2 / cores |
| CPU, 4–7 cores | base | small | cores−2 / cores |
| CPU, <4 cores | base | base (suggest groq) | cores−2 / cores |
| RAM < 3 GB | base | base | — |

The heuristic is a starting point. A real `init --calibrate` should run the
`examples/RTF-VAD` benchmark and replace the model picks with measured RTF,
since core count alone mis-predicts RTF (whisper's fixed 30 s-window encode cost).

## Result on the dev box

Ryzen 7 7730U (8c/16t, CPU-only, 15 GB) → live=`small`/6 threads,
batch=`large-v3-turbo`/8 threads. Matches the benchmarked decision.
