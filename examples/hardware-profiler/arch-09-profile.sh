#!/usr/bin/env bash
#
# arch-09-profile  —  prototype of the CLI's install-time `init` step.
#
# Explores the machine's hardware, decides sensible default parameters for it,
# and writes a machine profile. The real CLI reads this profile so casual users
# never have to pass flags. Precedence at runtime:
#     CLI flag  >  machine profile (this file)  >  built-in default
#
# Heuristic only. A real `init --calibrate` can additionally run a short RTF
# benchmark (see examples/RTF-VAD) and overwrite the model picks with measured
# numbers — more robust than guessing from core count.
#
# Output: JSON at $TINTO_PROFILE (default ~/.config/tinto/profile.json).
#
set -euo pipefail

PROFILE="${TINTO_PROFILE:-$HOME/.config/tinto/profile.json}"

# ---------- explore hardware ----------
os="$(uname -s)"
distro="$( (. /etc/os-release 2>/dev/null && echo "$ID") || echo unknown)"
cpu="$(lscpu 2>/dev/null | awk -F: '/Model name/{gsub(/^ +/,"",$2);print $2;exit}')"
cores_phys="$(lscpu 2>/dev/null | awk -F: '/^Core\(s\) per socket/{c=$2} /^Socket\(s\)/{s=$2} END{gsub(/ /,"",c);gsub(/ /,"",s); print (c*s>0)?c*s:0}')"
threads_log="$(nproc 2>/dev/null || echo 1)"
[ "${cores_phys:-0}" -gt 0 ] 2>/dev/null || cores_phys="$threads_log"
ram_gb="$(awk '/MemTotal/{printf "%d", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo 0)"
has_avx2="$(grep -qm1 avx2 /proc/cpuinfo 2>/dev/null && echo true || echo false)"
gpu_desc="$(lspci 2>/dev/null | grep -iE 'vga|3d|display' | sed 's/.*: //' | paste -sd'; ' - || echo '')"

accel="cpu"
command -v nvidia-smi >/dev/null 2>&1 && accel="cuda"
command -v rocminfo   >/dev/null 2>&1 && accel="rocm"

# ---------- decide defaults ----------
threads_live=$(( cores_phys > 2 ? cores_phys - 2 : 1 ))
threads_batch=$(( cores_phys > 0 ? cores_phys : 1 ))

if [ "$accel" != "cpu" ]; then
    model_live="large-v3-turbo"; model_batch="large-v3-turbo"
    threads_live="$threads_batch"
    note="GPU acceleration ($accel): turbo is fast enough for live."
elif [ "$cores_phys" -ge 8 ]; then
    model_live="small"; model_batch="large-v3-turbo"
    note="8+ CPU cores: small keeps real-time for live, turbo for batch quality."
elif [ "$cores_phys" -ge 4 ]; then
    model_live="base"; model_batch="small"
    note="4-7 CPU cores: base for live, small for batch. Consider init --calibrate."
else
    model_live="base"; model_batch="base"
    note="Few CPU cores: base only; consider --backend groq for quality."
fi

# low RAM caps model size
if [ "${ram_gb:-0}" -lt 3 ] 2>/dev/null; then
    model_live="base"; model_batch="base"
    note="$note Low RAM (<3 GB): capped at base."
fi

# ---------- write profile ----------
mkdir -p "$(dirname "$PROFILE")"
cat > "$PROFILE" <<JSON
{
  "schema": 1,
  "generated": "$(date -Iseconds)",
  "hardware": {
    "os": "$os",
    "distro": "$distro",
    "cpu": "$cpu",
    "cores_physical": $cores_phys,
    "threads_logical": $threads_log,
    "ram_gb": $ram_gb,
    "avx2": $has_avx2,
    "gpu": "$gpu_desc",
    "accel": "$accel"
  },
  "defaults": {
    "quality_live": "$model_live",
    "quality_batch": "$model_batch",
    "threads_live": $threads_live,
    "threads_batch": $threads_batch,
    "window_sec": 30,
    "overlap_sec": 5,
    "storage_mode3": "ram",
    "storage_persist": "disk",
    "format": "wav",
    "backend": "local",
    "diarize": false,
    "word_timestamps": false,
    "lang": "es"
  }
}
JSON

echo "=== Hardware profile written to $PROFILE ==="
echo "  CPU         : ${cpu:-unknown} (${cores_phys}c / ${threads_log}t, AVX2=$has_avx2)"
echo "  RAM         : ${ram_gb} GB"
echo "  GPU / accel : ${gpu_desc:-none} -> $accel"
echo "  OS          : $os / $distro"
echo "--- chosen defaults ---"
echo "  live  : model=$model_live  threads=$threads_live"
echo "  batch : model=$model_batch threads=$threads_batch"
echo "  window=30s overlap=5s  lang=es  backend=local"
echo "  reason: $note"
echo ""
echo "Users can now run the CLI with no flags; any flag overrides this profile."
