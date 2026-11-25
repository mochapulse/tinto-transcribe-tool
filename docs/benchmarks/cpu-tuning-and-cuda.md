# Colombian Spanish tuning results

## Accepted change

Commit `95ad47b` bounds repeated VAD work during long pauses without trimming into a deferred utterance. The native Windows suite passed 7/7 tests with Whisper and diarization and 5/5 with inference disabled. The new no-model regression test failed on the v0.1.0 implementation as expected.

For one 93-second control (two voiced excerpts separated by 80 seconds of digital silence), observed VAD processing time fell from 39.09 to 4.50 seconds. Both outputs contained four speech intervals. The test was not an isolated CPU timing benchmark. In four real one-minute clips, the release and the refined build produced identical VAD intervals and identical timestamped transcripts.

## Decoder experiments

Holding the Small Q5 model fixed, fewer greedy candidates, a shorter encoder context, and disabling temperature fallback did not yield a useful improvement on the screening clips. Preserving original samples inside the same VAD windows or using beam search helped particular clips but failed to generalize to two distinct held-out clips. Small Q8 was not more accurate. The installed model defaults were not changed.

## Native Windows CUDA experiment

The GPU comparison used the standalone [upstream CUDA build](https://github.com/ggml-org/whisper.cpp/releases/tag/b5454) on a GeForce RTX 3050 Laptop GPU, not a CUDA build of tinto. CUDA use was confirmed in the Whisper logs. In two paired 30-second Turbo Q5 tests, process wall time was 2.35–2.89 s using CUDA versus 26.33–58.44 s using CPU. CPU results may include contention from the tuning batch. GPU and CPU did not produce identical text in those pairs.

The model screen used four one-minute excerpts without tinto's VAD or audio capture. Reserve clips differ from the tuning clips. WER is the sum of word substitutions, deletions, and insertions divided by the reference word count. The tallies are indicative because the conversations include overlapping speakers and the reference includes transcription conventions.

| GPU setting | Tuning WER | Held-out WER | Wall time per 60 s |
| --- | ---: | ---: | ---: |
| turbo | 39.78% | 42.56% | 3.52–10.24 s |
| turbo-no-flash | 38.32% | 45.54% | 4.53–8.87 s |
| large-v3 | 34.67% | 45.24% | 6.39–8.13 s |

Turbo was the better starting point for this GPU integration experiment. Large v3 led on the tuning cuts but lost on the held-out aggregate. All processing ran from files, with no playback or microphone use.

Audio and aligned reference from Corpus Ameresco, Universitat de València, [Barranquilla](https://corpusameresco.org/conversacion/BAQ_001_03_16) and [Medellín](https://corpusameresco.org/conversacion/MDE_001_02_12).
