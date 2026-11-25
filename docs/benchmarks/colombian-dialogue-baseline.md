# Tinto v0.1.0 Colombian Spanish offline benchmark

**Status:** Stopped at the maintainer’s request.
Runs finished: 8/8 excerpts, 0/8 full recordings, 6/8 controls, and 2/2 speaker trials.

- [Ameresco-Barranquilla, BAQ_001_03_16](https://corpusameresco.org/conversacion/BAQ_001_03_16). Subcorpus coordinated by Yolanda Rodríguez Cadena.
- [Ameresco-Medellín, MDE_001_02_12](https://corpusameresco.org/conversacion/MDE_001_02_12). Subcorpus coordinated by Ji Son Jang and Ana García Tesoro.
- Corpus Ameresco, coordinated by Marta Albelda and Maria Estellés, Universitat de València, ISSN 2659-8337.

## Method

Native Windows 11 x86-64, Intel Core i5-12450H, CPU only. A disposable harness built from the v0.1.0 core fed each audio file as mono 16 kHz frames into the Silero gate and 30-second Whisper window assembler. Whisper used six threads, Spanish, and the installed quantized models. The executable ran below normal priority and used AVX2, as the installed executable does. Nothing was played, captured, or sent to an external service for inference.

Word error rate (WER) is (substitutions + deletions + insertions) / reference words. Case, punctuation, ELAN notation, and explicit incomplete words were excluded. Regional forms and accents were retained. ELAN speaker turns were ordered by start time; overlapping or untranscribable audio and editorial redactions can inflate WER. The comparison windows are 03:00–05:00 in Barranquilla and 16:00–18:00 in Medellín. Full-length ASR runs were planned but did not finish.

Real-time factor (RTF) is end-to-end offline processing time divided by input duration, including model load, VAD and Whisper, excluding file decode. Below 1.0 means processing was faster than the input duration on average; it does not measure live capture latency, mixer behavior, control events, or CPU load under actual use.

Speaker detection uses the release's Sherpa-ONNX models, two CPU threads and automatic speaker count. It runs on VAD-aligned input with gaps preserved. Two scoring protocols are shown: excluding overlap and a ±250 ms boundary collar, and scoring overlap without a collar. Speaker IDs are matched to the reference by maximum total overlap. This measures speaker turns, not the accuracy of the speaker labels assigned to each text segment.

## Matched excerpt comparison

| Source | Quality | Audio (s) | WER | RTF | Reference words |
| --- | --- | --- | --- | --- | --- |
| Barranquilla | base | 120.0 | 74.92% | 0.2204 | 331 |
| Barranquilla | medium | 120.0 | 74.02% | 0.8514 | 331 |
| Barranquilla | small | 120.0 | 61.63% | 0.3507 | 331 |
| Barranquilla | turbo | 120.0 | 54.38% | 1.2612 | 331 |
| Medellín | base | 120.0 | 43.67% | 0.2008 | 300 |
| Medellín | medium | 120.0 | 29.0% | 1.0277 | 300 |
| Medellín | small | 120.0 | 34.0% | 0.2982 | 300 |
| Medellín | turbo | 120.0 | 34.0% | 1.2474 | 300 |

| Quality | Combined WER | Combined RTF |
| --- | --- | --- |
| turbo | 44.7% | 1.25 |
| small | 48.5% | 0.32 |
| medium | 52.6% | 0.94 |
| base | 60.1% | 0.21 |

## Full conversations

No full-length ASR runs finished before the batch was stopped. The speaker trials below are separate and used the complete recordings.

## Silence and noise controls

Controls are 30 seconds of silence or low-level Gaussian white noise. The SNR comparisons add seeded white noise to the same two-minute Medellín excerpt. WER is undefined when the reference contains no words.

| Input | Quality | WER | Output words | Detected speech (s) |
| --- | --- | --- | --- | --- |
| Medellín snr0 | small | 54.33% | 246 | 102.96 |
| Medellín snr10 | small | 41.67% | 247 | 105.14 |
| noise | small | n/a | 0 | 0 |
| noise | turbo | n/a | 0 | 0 |
| silence | small | n/a | 0 | 0 |
| silence | turbo | n/a | 0 | 0 |

## Speaker diarization

| Source | Speakers detected / annotated | Overlap scored | DER | Missed speech | False alarm | Speaker confusion |
| --- | --- | --- | --- | --- | --- | --- |
| Barranquilla | 29 / 3 | False | 75.05% | 25.32% | 2.75% | 46.98% |
| Barranquilla | 29 / 3 | True | 83.31% | 34.94% | 5.58% | 42.78% |
| Medellín | 112 / 2 | False | 79.97% | 9.86% | 2.64% | 67.47% |
| Medellín | 112 / 2 | True | 89.26% | 20.07% | 6.77% | 62.43% |

The two recordings cover coastal and Andean Colombian speech but do not establish accuracy for every Colombian accent or for a live meeting.
This report contains aggregate counts only. No audio or reference transcript text is reproduced.
