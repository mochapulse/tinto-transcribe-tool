# Tinto on Windows and CPU-only Arch

Use the published [v0.1.0 release](https://github.com/mochapulse/tinto-transcribe-tool/releases/tag/v0.1.0) on both machines. On Windows, keep the verified CPU installation with `small` for live transcription. On Arch, initialize a fresh profile and choose `base` if `small` cannot keep up. CUDA and audio separation are experiments, not v0.1.0 features.

## Windows 11

The installed x64 release was checked natively with `tinto -v` and `tinto health`. It reported version `0.1.0`, all seven required models ready, WASAPI endpoints, and an idle worker. The profile uses local inference, `small`, six live threads, and 30-second windows. The published binary uses CPU inference even on the tested RTX 3050 Laptop GPU.

For system-audio transcription with no saved WAV, use PowerShell:

```powershell
tinto health
tinto record -m 3 -s system -q small -l es
tinto status
# Run this when the session is finished.
tinto stop
```

The recording runs in the background by default. Use `tinto stop` to finalize it without relying on console interrupt handling. Change `-s system` to `-s input` for a microphone or `-s both` to require both inputs. Use `-d NAME` to select a microphone by a name substring.

No reinstall is needed while the current installation is healthy. To reinstall the same release on another Windows 11 x64 machine:

```powershell
$installer = Join-Path $env:TEMP 'tinto-install-0.1.0.ps1'
Invoke-WebRequest 'https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/v0.1.0/install/install.ps1' -OutFile $installer
powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -Version v0.1.0
```

The packaged command is under `%LOCALAPPDATA%\Programs\tinto` by default. Open a new PowerShell session if the command is not on the current session's PATH.

## CPU-only Arch Linux

Install the published Linux x86_64 build as the desktop user. The installer detects Arch, installs missing runtime packages, verifies the release archive, and runs `init` to write a machine-specific profile and download the required local models. It may invoke `sudo` for system packages. Do not copy the Windows profile to Arch. No compiler or GPU toolkit is required. Provision `curl` with the system package manager first if it is not already installed.

```bash
curl -fsSLo tinto-install.sh https://raw.githubusercontent.com/mochapulse/tinto-transcribe-tool/v0.1.0/install/install.sh
bash tinto-install.sh --version v0.1.0
export PATH="$HOME/.local/bin:$PATH"
tinto -v
tinto health
```

Run capture in the logged-in user's audio session, not as root. Linux system capture reads the default sink monitor through PulseAudio or the PipeWire PulseAudio-compatible server. Keep the existing working audio server. If no endpoints are reported on an Arch PipeWire desktop, check `pactl info` and the user services for [pipewire-pulse](https://archlinux.org/packages/extra/x86_64/pipewire-pulse/) and [WirePlumber](https://archlinux.org/packages/extra/x86_64/wireplumber/) before changing Tinto settings.

```bash
tinto record -m 3 -s system -q small -l es
tinto status
# Run this when the session is finished.
tinto stop
```

For an initial low-power test start with `-q base`, or switch to it if `small` leaves a transcription backlog. Leave thread count on the local profile at first and tune `-t N` on that machine only if needed. Use a short real recording to check that new text keeps up and `stop` finalizes the Markdown. Windows benchmark speeds do not establish throughput on the Arch PC. The GitHub workflow tests the Arch container installer, not audio routing or real-time inference on that desktop.

If even `base` cannot keep up, use `tinto record -m 1 -s system` to save audio without running Whisper. v0.1.0 has no supported file-to-text command to transcribe that WAV later.

## Saved audio and speaker labels

On either platform, use mode 2 with `--diarize` if a session needs speaker labels. This keeps a time-aligned WAV and runs speaker analysis only when the separate `diarize` command is called after recording.

```sh
tinto record -m 2 -s system -q small -l es --diarize
tinto stop
tinto diarize --speakers 2
```

Set `--speakers 2` only when two speakers are expected. Automatic counting fragmented the two tested conversations into too many clusters. Diarization labels already transcribed segments and does not unmix two simultaneous voices. Mode 3 does not retain the WAV needed for later labeling.

Recordings and Markdown are stored under `recordings/` and `transcripts/` in `~/.tinto/`, respectively. `TINTO_HOME` overrides the data directory.

## Continue development from this point

1. Read the [benchmark index](benchmarks/README.md). The quiet-period VAD fix is commit `95ad47b`, newer than v0.1.0, and was tested on Windows. The official installation was not replaced with that build.
2. Prototype a separate optional Windows CUDA build for `turbo`. The GPU speed results came from standalone Whisper, not an integrated Tinto build. Keep CPU-only Windows and Linux paths working.
3. If adding speech separation, start with opt-in post-processing on saved audio. Validate Libri2Mix as the lower-cost option and MossFormer2 as the higher-accuracy option. Preserve the original audio, test single-voice false splits, and validate detection and speaker continuity on real meetings before enabling any automatic or live path.

No model weights, recordings, reference transcripts, or temporary harnesses are committed. The archived reports include methods, source links, measurements, and limits.
