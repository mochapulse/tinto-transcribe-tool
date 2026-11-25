# Download the benchmark audio and references

Start with the two Ameresco recordings to test realistic dialogue. Use FLEURS and OpenSLR as clear-speech controls, and the UNAM audiobook as a continuous narration control. Download the files to a work directory outside this repository. The repository keeps only links, selection metadata, and aggregate results.

## Spontaneous conversation

| Recording | Audio | Aligned reference | Baseline excerpt |
| --- | --- | --- | --- |
| [Barranquilla, BAQ_001_03_16](https://corpusameresco.org/conversacion/BAQ_001_03_16) | [MP3](https://corpusameresco.org/archivos/audio/BAQ_001_03_16.mp3) | [ELAN](https://corpusameresco.org/archivos/eaf/BAQ_001_03_16.eaf) · [TextGrid](https://corpusameresco.org/archivos/textgrid/BAQ_001_03_16.TextGrid) | 03:00–05:00 |
| [Medellín, MDE_001_02_12](https://corpusameresco.org/conversacion/MDE_001_02_12) | [MP3](https://corpusameresco.org/archivos/audio/MDE_001_02_12.mp3) | [ELAN](https://corpusameresco.org/archivos/eaf/MDE_001_02_12.eaf) · [TextGrid](https://corpusameresco.org/archivos/textgrid/MDE_001_02_12.TextGrid) | 16:00–18:00 |

Use ELAN for timed speaker tracks and overlap scoring. The corpus pages describe transcription conventions and provide attribution. Full-length ASR was not completed in the [original baseline](colombian-dialogue-baseline.md).

## Clear read speech

| Corpus and split | Audio | Reference |
| --- | --- | --- |
| [Google FLEURS](https://huggingface.co/datasets/google/fleurs), Latin American Spanish `es_419`, test | [WAV archive (582 MB)](https://huggingface.co/datasets/google/fleurs/resolve/70bb2e84b976b7e960aa89f1c648e09c59f894dd/data/es_419/audio/test.tar.gz?download=true) | [test.tsv](https://huggingface.co/datasets/google/fleurs/resolve/70bb2e84b976b7e960aa89f1c648e09c59f894dd/data/es_419/test.tsv) |
| [Google OpenSLR SLR72](https://www.openslr.org/72), Colombian Spanish | [Female voices (ZIP)](https://openslr.trmal.net/resources/72/es_co_female.zip) · [Male voices (ZIP)](https://openslr.trmal.net/resources/72/es_co_male.zip) | [Female TSV](https://openslr.trmal.net/resources/72/line_index_female.tsv) · [Male TSV](https://openslr.trmal.net/resources/72/line_index_male.tsv) |

The [read clip manifest](read-clips.csv) lists the exact 36 clips per corpus, their order, and the positions in the assembled fixtures. Match the FLEURS WAV filename in `test.tsv` and the OpenSLR file stem in the relevant TSV. The fixtures use mono 16 kHz PCM and a 350 ms gap after each clip. For the target-only overlap control, the first 12 FLEURS clips are the target. Each is mixed with the clip 18 rows later in the FLEURS manifest, at 3 dB below the target RMS. See the [clear-speech report](latin-american-spanish-controls.md) for scoring limits.

FLEURS is CC BY 4.0 and OpenSLR SLR72 is CC BY-SA 4.0. The pinned FLEURS audio archive advertises SHA-256 `981802f6c828fd214fcf8bfc1036d80c9184b6eeb5650b3f7882f8affec046c9`.

## Continuous narration

- [UNAM audiobook record](https://librosoa.unam.mx/handle/123456789/651) and [direct MP3](https://librosoa.unam.mx/bitstream/handle/123456789/651/21.mp3?sequence=2&isAllowed=y). The complete recording is about 12 minutes.
- [Published text of *El almohadón de plumas*](https://elcronistaaudiolibros.com/episodes/almohadon/). This is a separate edition, not a time-aligned transcript. The scored narration excludes the introduction and credits, using an approximate 00:45–11:45 bracket.

## Separation benchmark

The short mixtures and continuous two-voice fixture were derived from independently recorded FLEURS and OpenSLR clips. There is no upstream archive with these exact mixtures. Use the [separation case manifest](separation-cases.csv) for source IDs, split membership, and mixture settings. `voice_b_db` is relative to voice A. The `room` flag marks synthetic reflections and low-level noise. Each natural excerpt is a slice of the Ameresco files above, with its original start time recorded in the manifest.

Run the clean inputs through the same recognizer first. Compare the plain mono mix and each separator with fixed recognition settings. Keep the held-out set separate from tuning, and retain the single-voice clips to check for false second tracks. See the [separation report](simultaneous-speech.md) for the scoring method and results.
