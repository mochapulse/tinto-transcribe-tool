#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace tinto {

struct SpeakerTurn {
  double start_sec;
  double end_sec;
  int cluster;
};

struct LabeledTranscript {
  std::string markdown;
  std::size_t speakers = 0;
  std::size_t labeled_segments = 0;
};

// Match transcript headings to turns on the same clock. Cluster labels are
// anonymous, understood only within this transcript, and assigned in order.
LabeledTranscript LabelTranscript(const std::string& markdown,
                                  std::vector<SpeakerTurn> turns);

// Offline local inference on mono 16 kHz PCM. Pass zero for automatic counting.
std::vector<SpeakerTurn> AnalyzeSpeakers(const std::vector<float>& samples,
                                         const std::filesystem::path& models,
                                         int threads, int speaker_count);

}  // namespace tinto
