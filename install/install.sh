#!/usr/bin/env bash
# tinto installer — one-line install for Linux x86_64:
#   curl -fsSL https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/main/install/install.sh | bash
#
# What it does:
#   1. Installs system dependencies (audio libs) if missing
#   2. Downloads the tinto binary from GitHub Releases
#   3. Installs to ~/.local/bin (or /usr/local/bin with --system)
#   4. Creates ~/.tinto/ layout
#   5. Runs tinto init (hardware profile)
#   6. Downloads ASR + VAD models based on the profile
#   7. Verifies everything works
#
# Options:
#   --system        Install to /usr/local/bin (needs sudo)
#   --minimal       Skip model download (tinto init only)
#   --version VER   Install a specific version (default: latest)
set -euo pipefail

REPO="mochapulse/tinto-transcribe-tool"
INSTALL_DIR="${HOME}/.local/bin"
TINTO_HOME="${TINTO_HOME:-${HOME}/.tinto}"
MINIMAL=false
VERSION=""
SYSTEM_INSTALL=false

# --- helpers ---------------------------------------------------------------

bold()  { printf '\033[1m%s\033[0m\n' "$*"; }
green() { printf '\033[1;32m%s\033[0m\n' "$*"; }
red()   { printf '\033[1;31m%s\033[0m\n' "$*"; }
warn()  { printf '\033[1;33mwarning:\033[0m %s\n' "$*"; }
die()   { red "error: $*" >&2; exit 1; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "'$1' is required but not found"
}

# --- parse args ------------------------------------------------------------

while [ $# -gt 0 ]; do
  case "$1" in
    --system)   SYSTEM_INSTALL=true; INSTALL_DIR="/usr/local/bin" ;;
    --minimal)  MINIMAL=true ;;
    --version)  shift; VERSION="${1:-}"; [ -z "$VERSION" ] && die "--version needs a value" ;;
    -h|--help)
      echo "Usage: install.sh [--system] [--minimal] [--version VER]"
      echo "  --system   Install to /usr/local/bin (needs sudo)"
      echo "  --minimal  Skip model download"
      echo "  --version  Specific release tag (default: latest)"
      exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
  shift
done

# --- detect platform -------------------------------------------------------

OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
  Linux)  ;;
  *)      die "unsupported OS: $OS (only Linux is supported for now)" ;;
esac

case "$ARCH" in
  x86_64) ;;
  *)      die "unsupported architecture: $ARCH (only x86_64 for now)" ;;
esac

ASSET_NAME="tinto-${VERSION:-latest}-linux-x86_64"

bold "=== tinto installer ==="
echo "  OS:       $OS / $ARCH"
echo "  install:  $INSTALL_DIR"
echo "  home:     $TINTO_HOME"
echo ""

# --- install system dependencies -------------------------------------------

install_deps() {
  bold ">> checking system dependencies..."

  local missing=()

  # miniaudio needs libpulse at runtime (dlopen). Check for the .so file
  # directly — ldconfig -p can return non-zero on some distros and pipefail
  # would treat the whole check as a miss.
  local pulse_found=false
  for dir in /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /lib /lib64; do
    if ls "${dir}"/libpulse.so* >/dev/null 2>&1; then
      pulse_found=true
      break
    fi
  done
  if [ "$pulse_found" = false ]; then
    missing+=("libpulse")
  fi

  # curl is needed for downloads.
  if ! command -v curl >/dev/null 2>&1; then
    missing+=("curl")
  fi

  if [ ${#missing[@]} -eq 0 ]; then
    echo "  all dependencies present"
    return 0
  fi

  echo "  missing: ${missing[*]}"

  # Detect package manager and install.
  if command -v pacman >/dev/null 2>&1; then
    local pkgs=()
    for dep in "${missing[@]}"; do
      case "$dep" in
        libpulse) pkgs+=("libpulse") ;;
        curl)     pkgs+=("curl") ;;
      esac
    done
    bold ">> installing via pacman: ${pkgs[*]}"
    sudo pacman -S --needed --noconfirm "${pkgs[@]}"

  elif command -v apt-get >/dev/null 2>&1; then
    local pkgs=()
    for dep in "${missing[@]}"; do
      case "$dep" in
        libpulse) pkgs+=("libpulse0" "pulseaudio") ;;
        curl)     pkgs+=("curl") ;;
      esac
    done
    bold ">> installing via apt: ${pkgs[*]}"
    sudo apt-get update -qq
    sudo apt-get install -y -qq "${pkgs[@]}"

  elif command -v dnf >/dev/null 2>&1; then
    local pkgs=()
    for dep in "${missing[@]}"; do
      case "$dep" in
        libpulse) pkgs+=("pulseaudio-libs") ;;
        curl)     pkgs+=("curl") ;;
      esac
    done
    bold ">> installing via dnf: ${pkgs[*]}"
    sudo dnf install -y "${pkgs[@]}"

  elif command -v zypper >/dev/null 2>&1; then
    local pkgs=()
    for dep in "${missing[@]}"; do
      case "$dep" in
        libpulse) pkgs+=("libpulse0") ;;
        curl)     pkgs+=("curl") ;;
      esac
    done
    bold ">> installing via zypper: ${pkgs[*]}"
    sudo zypper install -y "${pkgs[@]}"

  else
    die "no supported package manager found (need pacman, apt, dnf, or zypper). Install manually: ${missing[*]}"
  fi

  echo "  dependencies installed"
}

install_deps

# --- resolve version -------------------------------------------------------

need_cmd curl

if [ -z "$VERSION" ]; then
  bold ">> resolving latest release..."
  VERSION="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" \
    | grep '"tag_name"' | head -1 | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/')"
  [ -z "$VERSION" ] && die "could not determine latest version"
fi

ASSET_NAME="tinto-${VERSION}-linux-x86_64"
DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${VERSION}/${ASSET_NAME}"

echo "  version: $VERSION"
echo "  url:     $DOWNLOAD_URL"

# --- download binary -------------------------------------------------------

bold ">> downloading tinto ${VERSION}..."
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

curl -fSL --progress-bar -o "${TMP}/tinto" "$DOWNLOAD_URL" \
  || die "download failed — check that ${VERSION} exists at ${DOWNLOAD_URL}"

chmod +x "${TMP}/tinto"

# Quick sanity check.
"${TMP}/tinto" --version >/dev/null 2>&1 \
  || die "downloaded binary failed to run — wrong architecture?"

BINARY_VERSION="$("${TMP}/tinto" --version)"
echo "  binary version: ${BINARY_VERSION}"

# --- install binary --------------------------------------------------------

bold ">> installing to ${INSTALL_DIR}..."
mkdir -p "$INSTALL_DIR"

if [ "$SYSTEM_INSTALL" = true ]; then
  sudo install -m 755 "${TMP}/tinto" "${INSTALL_DIR}/tinto"
else
  install -m 755 "${TMP}/tinto" "${INSTALL_DIR}/tinto"
fi

# Check PATH.
if ! echo "$PATH" | tr ':' '\n' | grep -qx "$INSTALL_DIR"; then
  warn "${INSTALL_DIR} is not in your PATH"
  echo "  add this to your shell rc (~/.bashrc, ~/.zshrc):"
  echo "    export PATH=\"${INSTALL_DIR}:\$PATH\""
  echo ""
  export PATH="${INSTALL_DIR}:$PATH"
fi

echo "  installed: $(command -v tinto)"

# --- create layout + profile -----------------------------------------------

bold ">> running tinto init..."
tinto init

# --- download models -------------------------------------------------------

if [ "$MINIMAL" = true ]; then
  warn "skipping model download (--minimal)"
else
  bold ">> downloading models..."

  MODELS_DIR="${TINTO_HOME}/models"
  PROFILE="${TINTO_HOME}/profile.json"

  HF_ASR="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
  HF_VAD="https://huggingface.co/ggml-org/whisper-vad/resolve/main"

  # Parse the profile to find which models to download.
  if [ -f "$PROFILE" ]; then
    QUALITY_LIVE="$(grep '"quality_live"' "$PROFILE" | head -1 | sed 's/.*: *"\([^"]*\)".*/\1/')"
    QUALITY_BATCH="$(grep '"quality_batch"' "$PROFILE" | head -1 | sed 's/.*: *"\([^"]*\)".*/\1/')"
  else
    QUALITY_LIVE="small"
    QUALITY_BATCH="large-v3-turbo"
  fi

  # Map quality names to model filenames.
  model_file() {
    case "$1" in
      base)           echo "ggml-base-q5_1.bin" ;;
      small)          echo "ggml-small-q5_1.bin" ;;
      medium)         echo "ggml-medium-q5_0.bin" ;;
      large-v3-turbo) echo "ggml-large-v3-turbo-q5_0.bin" ;;
      *)              echo "ggml-${1}.bin" ;;
    esac
  }

  download_model() {
    local url="$1"
    local dest="$2"
    local name
    name="$(basename "$dest")"

    if [ -f "$dest" ]; then
      echo "  $name already present, skipping"
      return 0
    fi

    echo "  downloading $name..."
    curl -fSL --progress-bar -o "${dest}.part" "$url" \
      || { rm -f "${dest}.part"; die "failed to download $name"; }
    mv "${dest}.part" "$dest"
    echo "  $name done ($(du -h "$dest" | cut -f1))"
  }

  mkdir -p "$MODELS_DIR"

  # VAD model (always needed for modes 2 and 3).
  VAD_FILE="ggml-silero-v5.1.2.bin"
  download_model "${HF_VAD}/${VAD_FILE}" "${MODELS_DIR}/${VAD_FILE}"

  # Live ASR model.
  LIVE_FILE="$(model_file "$QUALITY_LIVE")"
  download_model "${HF_ASR}/${LIVE_FILE}" "${MODELS_DIR}/${LIVE_FILE}"

  # Batch ASR model (skip if same as live).
  BATCH_FILE="$(model_file "$QUALITY_BATCH")"
  if [ "$BATCH_FILE" != "$LIVE_FILE" ]; then
    download_model "${HF_ASR}/${BATCH_FILE}" "${MODELS_DIR}/${BATCH_FILE}"
  fi

  echo "  models in ${MODELS_DIR}:"
  ls -lh "${MODELS_DIR}/"*.bin 2>/dev/null | while read -r line; do
    echo "    $line"
  done
fi

# --- verify ----------------------------------------------------------------

bold ">> verifying installation..."

echo "  tinto version: $(tinto --version)"
echo "  tinto status:"
tinto status 2>&1 | sed 's/^/    /' || true

echo ""
green "=== tinto installed successfully ==="
echo ""
echo "  Quick start:"
echo "    tinto record          # start recording (background)"
echo "    tinto record -f       # foreground (see output live)"
echo "    tinto status          # check worker status"
echo "    tinto stop            # stop and save files"
echo ""
echo "  Modes:"
echo "    tinto record --mode 1   # record only (WAV)"
echo "    tinto record --mode 2   # record + transcribe (default)"
echo "    tinto record --mode 3   # transcribe only (no audio saved)"
echo ""
echo "  Files:"
echo "    recordings:  ${TINTO_HOME}/recordings/"
echo "    transcripts: ${TINTO_HOME}/transcripts/"
echo "    logs:        ${TINTO_HOME}/logs/"
echo "    models:      ${TINTO_HOME}/models/"
echo ""
