# Benchmark handoff

Use these reports to choose the next experiment, not to infer live throughput or accuracy across all recordings. The tests processed files on native Windows 11 and did not exercise the microphone or WASAPI loopback. For shipping behavior and commands, start with the [platform handoff](../platform-handoff.md).

To run another test, start with the [audio and reference downloads](audio-sources.md). The page also links to the exact clip and case selections.

| Report | What it establishes |
| --- | --- |
| [Colombian conversation baseline](colombian-dialogue-baseline.md) | All four published Whisper presets were compared on matched spontaneous excerpts. Full-length ASR was stopped before completion. The separate full-length diarization runs show severe over-clustering with automatic speaker counts. |
| [CPU tuning and CUDA](cpu-tuning-and-cuda.md) | The quiet-period VAD change ships in v0.1.1. Decoder experiments did not justify changing defaults. Standalone CUDA was faster, but is not integrated into Tinto. |
| [Latin American Spanish controls](latin-american-spanish-controls.md) | Clear read speech was much easier than spontaneous conversation. Adding a competing voice to an otherwise unchanged sample caused a large error increase. |
| [Simultaneous speech separation](simultaneous-speech.md) | Four separators were screened. Libri2Mix and MossFormer2 FP32 improved held-out mixtures and two real excerpts. Both could emit a false second track with one speaker, and overlap detection on real recordings remains a blocker for automatic activation. |

The initial reports use v0.1.0 at `1e7c253`. Later experiments identify any changes or separate runtimes. WER, target-only WER, two-stream cpWER, and diarization error rate measure different tasks. Compare numbers within the same experiment and scoring protocol.
