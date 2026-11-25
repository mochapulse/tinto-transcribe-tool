# Latin American Spanish and overlapping speech

**Status:** Complete.

Windows 11 inference from files only. No playback, microphone, or external inference API.

The primary comparison uses the exact same v0.1.0 VAD/window/Whisper CPU harness, Small Q5, and six threads as the earlier Barranquilla and Medellín conversation excerpts. On those excerpts Small scored 61.63% and 34.00% WER, respectively. The standalone CUDA results are reported independently and do not exercise tinto's VAD.

The two clear speech sets contain 36 distinct read prompts each, 18 clips per recorded gender and 9–18 words per prompt, assembled with 350 ms gaps. They are [Google FLEURS, es_419 test set](https://huggingface.co/datasets/google/fleurs/tree/main/data/es_419) and [Google OpenSLR SLR72, Colombian Spanish](https://www.openslr.org/72). Neither selection establishes an accent-free norm. Speaker and recording environments differ between corpora, so the cross-corpus contrast does not isolate accent.

The paired interference control uses the same 12 FLEURS target clips before and after mixing unrelated FLEURS voices 3 dB below the target RMS. Only the original target speech is in the reference. Correctly recognized background speech can therefore count as insertions. This tests target transcription under interference, not all-speaker transcription or diarization.

| Engine | Model | Input | Audio (s) | Reference words | WER |
| --- | --- | --- | ---: | ---: | ---: |
| tinto core (CPU) | small | FLEURS Latin American Spanish, read | 311.46 | 521 | 7.29% |
| tinto core (CPU) | small | OpenSLR Colombian Spanish, read | 223.71 | 442 | 11.54% |
| tinto core (CPU) | small | FLEURS fixed target, one voice at a time | 98.46 | 170 | 6.47% |
| tinto core (CPU) | small | Same target with a competing voice | 98.46 | 170 | 74.12% |
| standalone Whisper (CUDA) | turbo | FLEURS Latin American Spanish, read | 311.46 | 521 | 3.65% |
| standalone Whisper (CUDA) | turbo | OpenSLR Colombian Spanish, read | 223.71 | 442 | 2.94% |
| standalone Whisper (CUDA) | turbo | FLEURS fixed target, one voice at a time | 98.46 | 170 | 2.94% |
| standalone Whisper (CUDA) | turbo | Same target with a competing voice | 98.46 | 170 | 24.12% |
| standalone Whisper (CUDA) | large-v3 | FLEURS Latin American Spanish, read | 311.46 | 521 | 3.07% |
| standalone Whisper (CUDA) | large-v3 | OpenSLR Colombian Spanish, read | 223.71 | 442 | 4.52% |
| standalone Whisper (CUDA) | large-v3 | FLEURS fixed target, one voice at a time | 98.46 | 170 | 3.53% |
| standalone Whisper (CUDA) | large-v3 | Same target with a competing voice | 98.46 | 170 | 45.29% |
| standalone Whisper (CUDA) | small | FLEURS Latin American Spanish, read | 311.46 | 521 | 6.53% |
| standalone Whisper (CUDA) | small | OpenSLR Colombian Spanish, read | 223.71 | 442 | 8.14% |
| standalone Whisper (CUDA) | small | FLEURS fixed target, one voice at a time | 98.46 | 170 | 7.06% |
| standalone Whisper (CUDA) | small | Same target with a competing voice | 98.46 | 170 | 81.18% |

WER counts substitutions, deletions and insertions divided by reference words, after case and punctuation normalization. Accents are retained in the main score. Strict WER may also count text form differences, such as written numbers.

In the [UNAM audiobook](https://librosoa.unam.mx/handle/123456789/651), scored speech spans covered 654.4 seconds of continuous narration. The standalone CUDA Turbo build had 5.76% indicative WER on 1215 words, versus the [published story text](https://elcronistaaudiolibros.com/episodes/almohadon/). Credits and introduction were excluded using an approximate timestamp bracket (00:45–11:45). This is a cross-edition literary reference, not a time-aligned transcript, and is not used as an accent control. The full 733-second file took 18.19 s of Whisper runtime.
