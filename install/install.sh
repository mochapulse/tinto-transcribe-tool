#!/usr/bin/env bash
# Install tinto on Debian-based or Arch-based x86_64 Linux.
set -euo pipefail

repo=mochapulse/tinto-transcribe-tool
version=
package=
minimal=false
system=false
bin_dir="$HOME/.local/bin"
app_dir="$HOME/.local/lib/tinto"

fail() { printf 'install: %s\n' "$*" >&2; exit 1; }
as_root() {
  if [[ $EUID -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null; then
    sudo "$@"
  else
    fail 'installing system packages requires root or sudo'
  fi
}

while (($#)); do
  case "$1" in
    -n|--minimal) minimal=true ;;
    -s|--system) system=true; bin_dir=/usr/local/bin; app_dir=/usr/local/lib/tinto ;;
    -v|--version) (($# >= 2)) || fail "$1 requires a release version"; version="$2"; shift ;;
    -p|--package) (($# >= 2)) || fail "$1 requires a local archive"; package="$2"; shift ;;
    -h|--help)
      cat <<'HELP'
Usage: install.sh [-n|--minimal] [-s|--system] [-v|--version VERSION] [-p|--package ARCHIVE]
  -n, --minimal          Skip the model downloads.
  -s, --system           Install the command under /usr/local/bin.
  -v, --version VERSION  Install a specific tag instead of the latest release.
  -p, --package ARCHIVE  Install a local archive with its .sha256 file alongside.
HELP
      exit 0 ;;
    *) fail "unknown option: $1" ;;
  esac
  shift
done

if [[ -n "$version" && "$version" != v* ]]; then version="v$version"; fi

[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || fail 'requires x86_64 Linux'
[[ -r /etc/os-release ]] || fail 'cannot detect the Linux distribution'
# shellcheck disable=SC1091
. /etc/os-release

case " ${ID:-} ${ID_LIKE:-} " in
  *' debian '*|*' ubuntu '*)
    family=debian
    dependencies=(ca-certificates curl tar gzip coreutils libpulse0 libgomp1 libstdc++6 util-linux)
    missing=()
    for dep in "${dependencies[@]}"; do
      [[ $(dpkg-query -W -f='${Status}' "$dep" 2>/dev/null || true) == 'install ok installed' ]] ||
        missing+=("$dep")
    done
    if ((${#missing[@]})); then
      as_root apt-get update
      as_root apt-get install -y --no-install-recommends "${missing[@]}"
    fi ;;
  *' arch '*|*' archlinux '*|*' manjaro '*|*' endeavouros '*)
    family=arch
    dependencies=(ca-certificates curl tar gzip coreutils libpulse gcc-libs util-linux)
    missing=()
    for dep in "${dependencies[@]}"; do
      pacman -Q "$dep" >/dev/null 2>&1 || missing+=("$dep")
    done
    if ((${#missing[@]})); then
      as_root pacman -S --needed --noconfirm "${missing[@]}" ||
        fail 'update the Arch system with pacman -Syu and retry'
    fi ;;
  *) fail "unsupported distribution: ${ID:-unknown}; use a Debian-based or Arch-based system" ;;
esac

printf 'install: detected %s (%s)\n' "$family" "${PRETTY_NAME:-Linux}"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

if [[ -z "$package" ]]; then
  if [[ -z "$version" ]]; then
    version=$(curl -fsSL "https://api.github.com/repos/$repo/releases/latest" |
      sed -n 's/^[[:space:]]*"tag_name":[[:space:]]*"\([^"]*\)".*/\1/p')
  fi
  [[ "$version" == v* ]] || version="v$version"
  [[ "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail 'invalid or unavailable release version'
  asset="tinto-$version-linux-x86_64.tar.gz"
  release="https://github.com/$repo/releases/download/$version"
  package="$tmp/$asset"
  curl -fSL --retry 3 -o "$package" "$release/$asset"
  curl -fsSL --retry 3 -o "$package.sha256" "$release/$asset.sha256"
fi

[[ -r "$package" && -r "$package.sha256" ]] || fail 'cannot read the archive and its .sha256 file'
read -r expected _ < "$package.sha256"
[[ "$expected" =~ ^[[:xdigit:]]{64}$ ]] || fail 'invalid package checksum'
actual=$(sha256sum < "$package")
[[ "${actual%% *}" == "${expected,,}" ]] || fail 'package checksum did not match'

mkdir "$tmp/bundle"
tar -xzf "$package" -C "$tmp/bundle"
exe="$tmp/bundle/tinto"
[[ -x "$exe" ]] || fail 'package does not contain tinto'
installed_version=$("$exe" -v)
if [[ -n "$version" && "v$installed_version" != "$version" ]]; then
  fail 'binary version does not match the requested release'
fi

if [[ "$system" == true ]]; then
  as_root mkdir -p "$bin_dir" "$app_dir"
  as_root cp -a "$tmp/bundle/." "$app_dir/"
  as_root ln -sfn "$app_dir/tinto" "$bin_dir/tinto"
else
  mkdir -p "$bin_dir" "$app_dir"
  cp -a "$tmp/bundle/." "$app_dir/"
  ln -sfn "$app_dir/tinto" "$bin_dir/tinto"
fi

if [[ "$minimal" == true ]]; then
  "$bin_dir/tinto" init -n
else
  "$bin_dir/tinto" init
  "$bin_dir/tinto" health
fi
printf 'install: tinto %s installed at %s/tinto\n' "$installed_version" "$bin_dir"
case ":$PATH:" in
  *":$bin_dir:"*) ;;
  *) printf 'Add %s to PATH or use %s/tinto directly.\n' "$bin_dir" "$bin_dir" ;;
esac
printf 'Run tinto --help for examples.\n'
