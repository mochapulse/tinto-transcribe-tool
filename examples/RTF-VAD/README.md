# RTF-VAD — whisper.cpp real-time + VAD probe

Validates the second half of the stack on this machine **before we commit**:
can whisper.cpp transcribe in (near) real time locally, and does its built-in
Silero VAD work? Each run records the hardware inline for traceability, because
RTF is meaningless without knowing the CPU/GPU it ran on.

## Why RTF matters here

This project prefers **local** transcription, but the dev box is **CPU-only**
(AMD Ryzen 7 7730U, integrated Vega, no CUDA/ROCm). `large-v3-turbo` may or may
not keep up with live audio on CPU. The **Real-Time Factor** decides it:

- `RTF < 1` → transcription is faster than the audio → live mode is viable.
- `RTF > 1` → slower than the audio → live mode must use a smaller model
  (medium/small/base), and turbo stays for batch/record modes.

## Steps

```sh
./build.sh                 # clone + build whisper.cpp, download ASR + VAD models (~1.5 GB, minutes)
./arch-06-whisper-rtf.sh   # measure RTF on this hardware (defaults to samples/jfk.wav)
./arch-07-vad-check.sh     # confirm built-in Silero VAD runs
```

Use your own 16 kHz mono WAV (e.g. the one from the capture suite's `arch-04`):

```sh
./arch-06-whisper-rtf.sh /tmp/mic.wav
```

Override the model to compare (e.g. a lighter one for live mode):

```sh
ASR_MODEL=medium-q5_0 ./build.sh
ASR_MODEL=medium-q5_0 ./arch-06-whisper-rtf.sh
```

## Traceability

- `arch-06` prints a hardware block (CPU, RAM, GPU, accel) and the exact
  `whisper.cpp` commit, and appends one TSV row per run to `results/rtf.log`, so
  measurements across machines and models stay comparable.
- `build.sh` pins the built commit in `whisper-commit.txt`.

## Ignored (not committed)

`whisper.cpp/` (source + build + models) and `results/` are git-ignored; they
are large and machine-specific.
