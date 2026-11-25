# Simultaneous Spanish speech separation on Windows

**Status:** Complete.

The older background batch and its worker were stopped before this run. All inference ran from files on Windows. No audio was played, captured, or sent to a hosted inference service. Models and derived audio remained outside the repository.

The test mixed separately recorded voices from [FLEURS Spanish (Latin America)](https://huggingface.co/datasets/google/fleurs/tree/main/data/es_419) and [OpenSLR Colombian Spanish](https://www.openslr.org/72). The short pairs have staggered starts, different voice levels, and include same-gender mixtures. Some held-out mixtures have synthetic room reflections and noise. Four single-voice clips test over-separation. A separate longer sequence uses two consistent speaker IDs and tests continuity across separator windows. It is not counted in the short held-out set.

The separators ran on CPU via ONNX Runtime. The downstream ASR used standalone Spanish Whisper on CUDA. Each short case is scored with the lowest total edit count over the two possible speaker assignments, divided by total reference words (cpWER). For the mono baseline, one output is empty and even the most favorable assignment is scored. The oracle transcribes the original separated inputs with the same recognizer. For the longer case, assignment is global over the whole case, not per window. Uppercase and punctuation are ignored; accents are retained.

## Separator screen and validation with Turbo

| Separation | Cases | Set | cpWER | Median SI-SDR improvement | CPU RTF |
| --- | ---: | --- | ---: | ---: | ---: |
| Mono, no separation | 4 | tune | 51.67% | — | — |
| Original separate tracks (oracle) | 4 | tune | 1.67% | — | — |
| SepFormer WHAMR 16 kHz | 4 | tune | 22.50% | 8.88 dB | 2.58 |
| SepFormer Libri2Mix 8 kHz | 4 | tune | 12.50% | 17.66 dB | 1.25 |
| MossFormer2 FP32 | 4 | tune | 1.67% | 22.44 dB | 5.42 |
| MossFormer2 INT8 export | 4 | tune | 1.67% | 22.42 dB | 6.82 |
| SepFormer Libri2Mix, 10-second window (exploratory) | 2 | tune | 24.59% | 10.96 dB | 0.82 |
| Mono, no separation | 8 | holdout | 57.00% | — | — |
| Original separate tracks (oracle) | 8 | holdout | 6.28% | — | — |
| SepFormer Libri2Mix 8 kHz | 8 | holdout | 8.21% | 14.66 dB | 0.94 |
| MossFormer2 FP32 | 8 | holdout | 6.28% | 19.22 dB | 4.14 |
| Mono, no separation | 1 | long | 91.18% | — | — |
| Original separate tracks (oracle) | 1 | long | 1.47% | — | — |
| SepFormer Libri2Mix 8 kHz | 1 | long | 7.35% | 20.09 dB | 1.16 |
| MossFormer2 FP32 | 1 | long | 1.47% | 25.49 dB | 5.43 |
| Mono, no separation | 2 | natural | 85.11% | — | — |
| SepFormer Libri2Mix 8 kHz | 2 | natural | 36.17% | — | — |
| MossFormer2 FP32 | 2 | natural | 25.53% | — | — |

RTF is separator CPU processing time divided by audio duration. Model load and downstream ASR are excluded. Some measurements ran alongside other tests. Values over one are slower than real time under the tested load.

## Single-voice regression control

| Separation | Cases | cpWER | Words transcribed on the unmatched second track |
| --- | ---: | ---: | ---: |
| Mono, no separation | 4 | 0.00% | 0 |
| Original separate tracks (oracle) | 4 | 0.00% | 0 |
| SepFormer Libri2Mix 8 kHz | 4 | 101.89% | 52 |
| MossFormer2 FP32 | 4 | 22.64% | 10 |

## Held-out ASR comparison

| Recognizer | Separation | Cases | cpWER |
| --- | --- | ---: | ---: |
| turbo | SepFormer Libri2Mix 8 kHz | 8 | 8.21% |
| turbo | Mono, no separation | 8 | 57.00% |
| turbo | MossFormer2 FP32 | 8 | 6.28% |
| turbo | Original separate tracks (oracle) | 8 | 6.28% |
| small | SepFormer Libri2Mix 8 kHz | 8 | 13.53% |
| small | Mono, no separation | 8 | 72.46% |
| small | MossFormer2 FP32 | 8 | 11.59% |
| large-v3 | SepFormer Libri2Mix 8 kHz | 8 | 8.70% |
| large-v3 | Mono, no separation | 8 | 61.84% |
| large-v3 | MossFormer2 FP32 | 8 | 5.80% |

## Overlap detection guard

A separate pilot used the existing int8 Pyannote segmentation model bundled with tinto. The threshold was set using only development pairs and one single-voice clip from each corpus. It was not used to choose a separator or to score the primary cpWER.

At the fixed threshold, it detected overlap at the clip level in 8/8 held-out mixtures and marked 0/2 held-out single-voice clips for separation.
- real-baq: frame recall 2.2% against overlapping Ameresco annotation intervals; these boundaries are coarser than the synthetic track references.
- real-mde: frame recall 8.1% against overlapping Ameresco annotation intervals; these boundaries are coarser than the synthetic track references.

The artificial mixtures do not establish that this guard is reliable on real meetings or on crowds of three or more speakers.

## Exploratory duplicate-text fallback

Fallback to the mono transcript triggered on 3/4 one-voice controls, reducing unmatched second-track words from 52 to 12. It did not replace any held-out mixed clip. A lexical check is insufficient as the only guard, and it may discard genuine speakers who repeat each other.

The natural rows are two short Ameresco excerpts selected before inference for annotated two-speaker overlap and few reference words cut by the edges. They provide an approximate spot check, not an evaluation of an entire real meeting.

## Model provenance

- [SepFormer WHAMR 16 kHz export](https://huggingface.co/tonythethompson/SepFormer-WhamR16k-ONNX)
- [SepFormer Libri2Mix export](https://huggingface.co/codebucketsolutions/sepformer-libri2mix-onnx)
- [MossFormer2 FP32 export](https://huggingface.co/iazShadow/mossformer2-ss-16k-onnx)
- [MossFormer2 INT8 export](https://huggingface.co/ipsilondev/MossFormer2-SS-16K-ONNX)
