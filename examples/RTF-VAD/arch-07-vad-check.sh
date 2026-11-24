#!/usr/bin/env bash
#
# arch-07-vad-check
#
# Confirms whisper.cpp's built-in Silero VAD works on this machine: runs the
# same audio twice (VAD off, then VAD on) and reports wall time for each. VAD
# should skip silence, so on audio with pauses the VAD run is usually faster.
# Records hardware inline for traceability.
#
# Usage: ./arch-07-vad-check.sh [audio.wav]
#
set -euo pipefail
cd "$(dirname "$0")"

SRC="whisper.cpp"
ASR_MODEL="${ASR_MODEL:-large-v3-turbo-q5_0}"
VAD_MODEL="${VAD_MODEL:-silero-v5.1.2}"
MODEL="$SRC/models/ggml-$ASR_MODEL.bin"
VAD="$SRC/models/ggml-$VAD_MODEL.bin"
CLI="$SRC/build/bin/whisper-cli"
WLANG="${LANG_CODE:-es}"
THREADS="${THREADS:-$(nproc)}"
INPUT="${1:-$SRC/samples/jfk.wav}"

[ -x "$CLI" ]   || { echo "missing $CLI — run ./build.sh first"; exit 1; }
[ -f "$MODEL" ] || { echo "missing $MODEL — run ./build.sh first"; exit 1; }
[ -f "$VAD" ]   || { echo "missing VAD model $VAD — run ./build.sh first"; exit 1; }
[ -f "$INPUT" ] || { echo "missing input audio: $INPUT"; exit 1; }

# ---- hardware snapshot (inline, for traceability) ----
hw_cpu="$(lscpu | awk -F: '/Model name/{gsub(/^ +/,"",$2); print $2; exit}')"
hw_accel="cpu-only"
command -v nvidia-smi >/dev/null 2>&1 && hw_accel="cuda"
command -v rocminfo  >/dev/null 2>&1 && hw_accel="rocm"
echo "=== Hardware (traceability) ==="
echo "  CPU: $hw_cpu | threads: $THREADS | accel: $hw_accel"
echo "  whisper.cpp commit: $(cat whisper-commit.txt 2>/dev/null || echo unknown)"
echo "  model: $ASR_MODEL | vad: $VAD_MODEL | input: $INPUT"
echo ""

run() { # $1 = label, rest = extra args
    local label="$1"; shift
    local s e
    s="$(date +%s.%N)"
    "$CLI" -m "$MODEL" -f "$INPUT" -l "$WLANG" -t "$THREADS" "$@" > "/tmp/whisper-$label.txt" 2>/dev/null
    e="$(date +%s.%N)"
    printf "  %-10s wall=%ss\n" "$label" "$(awk "BEGIN{printf \"%.2f\", $e-$s}")"
    echo "    transcript: $(tr -s ' \n' ' ' < /tmp/whisper-$label.txt | cut -c1-80)"
}

echo "=== VAD off vs on ==="
run "no-vad"
run "vad" --vad --vad-model "$VAD"
echo ""
echo "If the VAD run produced a transcript with no error, built-in Silero VAD works."
