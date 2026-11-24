# examples — Arch probes

Short programs that validate the stack **before we commit to it**. They test the
real doubts on this machine (Arch, PipeWire 1.6.9 w/ PulseAudio compat) using the
pinned `vendor/miniaudio.h` (v0.11.22). Not production code.

## Build

```sh
./build.sh          # builds all arch-*.c into ./build/
```

## Probes

| Probe | Question it answers | Needs a human? |
| ----- | ------------------- | :------------: |
| `arch-01-list-devices` | Can we see the backend + enumerate devices, including `.monitor` sources? | no |
| `arch-02-mic-shared`   | Can we capture the mic in **shared** mode (non-intrusive) at 16 kHz mono? | yes — speak |
| `arch-03-loopback-monitor` | Can we passively tap **system output** via a sink monitor (Linux loopback)? | yes — play audio |
| `arch-04-record-wav` | Objective proof: record a device to WAV, print peak/RMS/non-silent %. | mic: yes; loopback: no |

```sh
./build/arch-01-list-devices
./build/arch-02-mic-shared        # then speak for 5 s
./build/arch-03-loopback-monitor  # then play music/video for 8 s
```

### Objective verification (arch-04)

Numbers, not bars. `arch-04` records to a WAV and prints peak / RMS /
non-silent %, so "is it really capturing?" has a yes/no answer (`verdict`).

**Loopback — fully automatic proof** (no human; nothing plays on your real
output). Generate a known tone, play it to the *analog* sink, capture that
sink's monitor, and confirm the captured RMS is non-zero:

```sh
SINK=alsa_output.pci-0000_03_00.6.analog-stereo   # adjust via arch-01 / pactl
ffmpeg -loglevel error -y -f lavfi -i "sine=frequency=440:duration=5" \
       -ar 16000 -ac 1 /tmp/tone.wav
./build/arch-04-record-wav "monitor of ryzen" 6 /tmp/loopback.wav &
sleep 0.6; paplay --device="$SINK" /tmp/tone.wav; wait
```

**Mic — needs a human** (we cannot inject sound into a physical mic):

```sh
./build/arch-04-record-wav default 5 /tmp/mic.wav   # speak during the 5 s
paplay /tmp/mic.wav                                 # play it back to confirm
```

Non-intrusive check: keep a call app using the mic/output while these run; both
must keep working, proving we never take exclusive ownership.

## Findings so far (2026-11-24, this Arch box)

- **arch-01 ✅** Backend = `PulseAudio`; 2 monitor sources enumerated
  (`Monitor of ...`). Passive output capture is available.
- **arch-03 ✅ (opens)** miniaudio opened the monitor as a normal **capture**
  device with no error — the key Linux loopback doubt is resolved. Levels read 0
  only because nothing was playing.
  - **Refinement needed:** the probe grabs the *first* monitor it finds. The real
    code must tap the monitor of the **current default sink** (here the output is
    a Bluetooth sink, whose monitor differs). Track the default sink and follow it
    when it changes.
- **arch-04 loopback ✅ (proven)** Played a 440 Hz tone to the analog sink and
  captured its monitor: peak 0.125, **RMS 0.081, 83.7% non-silent**, verdict
  `SIGNAL CAPTURED`. The passive output tap really reads audio, not just opens.
- **arch-02 / arch-04 mic ✅ (proven)** Default mic: peak 0.95, RMS 0.145,
  98.2% non-silent, `SIGNAL CAPTURED`.
- **arch-05 multi-input ✅ (proven)** Opened two mics at once (analog + BT
  HAYLOU). One `ma_device` per source, all shared mode → simultaneous multi-mic
  capture works. Analog read SIGNAL.
- **⚠ Bluetooth mic caveat (discovered):** capturing a BT headset mic is
  inherently intrusive — the OS must switch the device from A2DP to HSP/HFP,
  which drops playback to telephone quality while the mic is open, and the first
  open stalled >120 s during profile negotiation. This is a Bluetooth-protocol
  limitation, not a miniaudio bug. Design stance: wired/USB mics and output
  monitors are clean passive taps; **treat BT mic capture as opt-in with a
  warning**, never a default.

## Not covered here

whisper.cpp (VAD + transcription) is its own CMake build with model downloads;
it is validated separately, not through these single-file probes.
