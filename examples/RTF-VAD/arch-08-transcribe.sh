#!/usr/bin/env bash
#
# arch-08-transcribe
#
# Prints the FULL transcript of a WAV (quality check on real Spanish audio),
# plus RTF and the hardware block. Unlike arch-06 (speed only) this shows the
# text, and unlike arch-07 it is not truncated. Use it on audio captured from a
# real podcast/meeting via the capture suite's arch-04.
#
# Usage: ./arch-08-transcribe.sh <audio.wav> [model]
#   audio.wav : 16 kHz mono WAV (arch-04 output)
#   model     : ggml model name (default: $ASR_MODEL or large-v3-turbo-q5_0)
#
set -euo pipefail
cd "$(dirname "$0")"

SRC="whisper.cpp"
ASR_MODEL="${2:-${ASR_MODEL:-large-v3-turbo-q5_0}}"
MODEL="$SRC/models/ggml-$ASR_MODEL.bin"
VAD_MODEL="${VAD_MODEL:-silero-v5.1.2}"
VAD="$SRC/models/ggml-$VAD_MODEL.bin"
CLI="$SRC/build/bin/whisper-cli"
WLANG="${LANG_CODE:-es}"
THREADS="${THREADS:-8}"
INPUT="${1:-}"

[ -n "$INPUT" ] || { echo "usage: $0 <audio.wav> [model]"; exit 2; }
[ -x "$CLI" ]   || { echo "missing $CLI — run ./build.sh first"; exit 1; }
[ -f "$MODEL" ] || { echo "missing $MODEL — run: ASR_MODEL=$ASR_MODEL ./build.sh"; exit 1; }
[ -f "$INPUT" ] || { echo "missing input audio: $INPUT"; exit 1; }

hw_cpu="$(lscpu | awk -F: '/Model name/{gsub(/^ +/,"",$2); print $2; exit}')"
hw_accel="cpu-only"
command -v nvidia-smi >/dev/null 2>&1 && hw_accel="cuda"
command -v rocminfo  >/dev/null 2>&1 && hw_accel="rocm"
echo "=== Hardware (traceability) ==="
echo "  CPU: $hw_cpu | threads: $THREADS | accel: $hw_accel"
echo "  whisper.cpp commit: $(cat whisper-commit.txt 2>/dev/null || echo unknown)"
echo "  model: $ASR_MODEL | lang: $WLANG | input: $INPUT"
echo ""

DUR="$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "$INPUT" 2>/dev/null || echo 0)"

VAD_ARGS=()
[ -f "$VAD" ] && VAD_ARGS=(--vad --vad-model "$VAD")

echo "=== Transcript ==="
START="$(date +%s.%N)"
"$CLI" -m "$MODEL" -f "$INPUT" -l "$WLANG" -t "$THREADS" "${VAD_ARGS[@]}"
END="$(date +%s.%N)"

echo ""
echo "=== Speed ==="
awk "BEGIN{printf \"  audio=%.1fs wall=%.1fs RTF=%.3f\n\", $DUR, $END-$START, ($END-$START)/($DUR>0?$DUR:1)}"
