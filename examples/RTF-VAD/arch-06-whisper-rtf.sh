#!/usr/bin/env bash
#
# arch-06-whisper-rtf
#
# Measures the Real-Time Factor (RTF) of whisper.cpp on THIS machine:
#   RTF = wall_time / audio_duration
#   RTF < 1  -> faster than real time (viable for live transcription)
#   RTF > 1  -> slower than real time (live mode needs a smaller model)
#
# Records the hardware inline for traceability, and appends a row to
# results/rtf.log so runs on different machines/models stay comparable.
#
# Usage: ./arch-06-whisper-rtf.sh [audio.wav]
#   audio.wav must be 16 kHz mono (arch-04 output works). Defaults to the
#   bundled samples/jfk.wav.
#
set -euo pipefail
cd "$(dirname "$0")"

SRC="whisper.cpp"
ASR_MODEL="${ASR_MODEL:-large-v3-turbo-q5_0}"
MODEL="$SRC/models/ggml-$ASR_MODEL.bin"
CLI="$SRC/build/bin/whisper-cli"
WLANG="${LANG_CODE:-es}"
THREADS="${THREADS:-$(nproc)}"
INPUT="${1:-$SRC/samples/jfk.wav}"

[ -x "$CLI" ]    || { echo "missing $CLI — run ./build.sh first"; exit 1; }
[ -f "$MODEL" ]  || { echo "missing $MODEL — run ./build.sh first"; exit 1; }
[ -f "$INPUT" ]  || { echo "missing input audio: $INPUT"; exit 1; }

mkdir -p results

# ---- hardware snapshot (inline, for traceability) ----
hw_cpu="$(lscpu | awk -F: '/Model name/{gsub(/^ +/,"",$2); print $2; exit}')"
hw_cores="$(nproc)"
hw_ram="$(free -h | awk '/^Mem:/{print $2}')"
hw_gpu="$(lspci | grep -iE 'vga|3d|display' | sed 's/.*: //' | paste -sd'; ' -)"
hw_accel="cpu-only"
command -v nvidia-smi >/dev/null 2>&1 && hw_accel="cuda"
command -v rocminfo  >/dev/null 2>&1 && hw_accel="rocm"
whisper_commit="$(cat whisper-commit.txt 2>/dev/null || echo unknown)"

echo "=== Hardware (traceability) ==="
echo "  CPU   : $hw_cpu ($hw_cores threads)"
echo "  RAM   : $hw_ram"
echo "  GPU   : ${hw_gpu:-none}"
echo "  Accel : $hw_accel"
echo "  whisper.cpp commit: $whisper_commit"
echo "  model : $ASR_MODEL   threads: $THREADS   lang: $WLANG"
echo "  input : $INPUT"
echo ""

# ---- audio duration ----
if command -v ffprobe >/dev/null 2>&1; then
    DUR="$(ffprobe -v error -show_entries format=duration \
           -of default=nw=1:nk=1 "$INPUT")"
else
    echo "ffprobe not found; cannot compute duration"; exit 1
fi

# ---- timed run ----
echo "=== Running whisper-cli ==="
START="$(date +%s.%N)"
"$CLI" -m "$MODEL" -f "$INPUT" -l "$WLANG" -t "$THREADS" -nt >/dev/null 2>&1
END="$(date +%s.%N)"

ELAPSED="$(awk "BEGIN{printf \"%.2f\", $END-$START}")"
RTF="$(awk "BEGIN{printf \"%.3f\", ($END-$START)/$DUR}")"
VERDICT="$(awk "BEGIN{print (($END-$START)/$DUR < 1.0) ? \"FASTER than real time (live OK)\" : \"SLOWER than real time (use smaller model for live)\"}")"

echo "  audio duration : ${DUR}s"
echo "  wall time      : ${ELAPSED}s"
echo "  RTF            : $RTF"
echo "  verdict        : $VERDICT"

# ---- append to log ----
TS="$(date -Iseconds)"
LOG="results/rtf.log"
if [ ! -f "$LOG" ]; then
    echo "timestamp	cpu	threads	ram	accel	model	lang	audio_s	wall_s	rtf	whisper_commit" > "$LOG"
fi
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$TS" "$hw_cpu" "$THREADS" "$hw_ram" "$hw_accel" "$ASR_MODEL" \
    "$LANG" "$DUR" "$ELAPSED" "$RTF" "$whisper_commit" >> "$LOG"
echo ""
echo "logged -> $LOG"
