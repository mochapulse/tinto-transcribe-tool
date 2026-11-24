#!/usr/bin/env bash
# Build the arch-* probe programs. Linux/miniaudio uses the PulseAudio backend
# (dlopened), so we only need pthread/m/dl. Output binaries land in ./build.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
CC="${CC:-cc}"
for src in arch-*.c; do
    bin="build/${src%.c}"
    echo "CC $src -> $bin"
    "$CC" -O2 -Wall -Wextra "$src" -o "$bin" -lpthread -lm -ldl
done
echo "Done. Binaries in examples/build/"
