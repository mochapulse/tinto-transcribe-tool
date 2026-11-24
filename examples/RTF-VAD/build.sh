#!/usr/bin/env bash
#
# Build whisper.cpp and fetch the models for the RTF + VAD benchmark.
# Everything lands under this folder (git-ignored). Heavy: a few minutes of
# compile and ~1.5 GB of model download for large-v3-turbo-q5_0.
#
# Override via env:
#   WHISPER_REF=master        git ref to check out (default: master, depth 1)
#   ASR_MODEL=large-v3-turbo-q5_0   ggml model name for download-ggml-model.sh
#   VAD_MODEL=silero-v5.1.2         vad model name for download-vad-model.sh
#
set -euo pipefail
cd "$(dirname "$0")"

WHISPER_REF="${WHISPER_REF:-master}"
ASR_MODEL="${ASR_MODEL:-large-v3-turbo-q5_0}"
VAD_MODEL="${VAD_MODEL:-silero-v5.1.2}"
REPO="https://github.com/ggml-org/whisper.cpp.git"
SRC="whisper.cpp"

if [ ! -d "$SRC/.git" ]; then
    echo ">> cloning whisper.cpp ($WHISPER_REF)"
    git clone --depth 1 --branch "$WHISPER_REF" "$REPO" "$SRC" 2>/dev/null \
        || git clone --depth 1 "$REPO" "$SRC"
else
    echo ">> whisper.cpp already present, reusing"
fi

# Record exactly which whisper.cpp commit we built, for traceability.
COMMIT="$(git -C "$SRC" rev-parse HEAD)"
echo ">> whisper.cpp commit: $COMMIT"
echo "$COMMIT" > whisper-commit.txt

echo ">> configuring (CMake, Release)"
cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
echo ">> compiling"
cmake --build "$SRC/build" -j --config Release >/dev/null

echo ">> downloading ASR model: $ASR_MODEL (~1.5 GB for turbo-q5_0)"
bash "$SRC/models/download-ggml-model.sh" "$ASR_MODEL"
echo ">> downloading VAD model: $VAD_MODEL"
bash "$SRC/models/download-vad-model.sh" "$VAD_MODEL"

echo ""
echo "Done."
echo "  whisper-cli : $SRC/build/bin/whisper-cli"
echo "  ASR model   : $SRC/models/ggml-$ASR_MODEL.bin"
echo "  VAD model   : $SRC/models/ggml-$VAD_MODEL.bin"
echo "Next: ./arch-06-whisper-rtf.sh   and   ./arch-07-vad-check.sh"
