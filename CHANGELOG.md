# Changelog

## [0.1.1](https://github.com/mochapulse/tinto-transcribe-tool/releases/tag/v0.1.1)

- Reduce repeated speech detection work during long pauses without cutting an unfinished utterance or moving its timestamps.
- Add a regression test for silence and deferred speech, plus [benchmark results and fixture downloads](docs/benchmarks/) for future comparisons.
- Document the tested CPU choices in the [platform guide](docs/platform-handoff.md). Start with `small` on Windows and use `base` on a CPU-only Arch machine when `small` cannot keep up. Automatic profiling and bundled model presets are unchanged.

CUDA acceleration and simultaneous-speaker separation remain experiments. Speaker labeling after recording is supported and does not separate overlapping audio.
