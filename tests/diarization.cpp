#include <filesystem>
#include <iostream>
#include <vector>

#include "tinto/diarization.hpp"
#include "tinto/storage.hpp"

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string original = "# Meeting Transcript\r\n\r\n"
      "**[00:00:00.500 — 00:00:01.000]**\r\nFirst turn.\r\n"
      "**[00:00:02.200 — 00:00:02.900]**\r\nOverlapping turn.\r\n"
      "**[00:00:03 — 00:00:04]**\r\nSecond turn.\r\n"
      "**[00:00:10.000 — 00:00:11.000]**\r\nUnattributed text.\r\n";
  const auto labeled = tinto::LabelTranscript(original, {{2, 4, 3}, {0, 3, 9}});
  if (labeled.speakers != 2 || labeled.labeled_segments != 3 ||
      labeled.markdown.find("01.000] · Speaker 1**\nFirst turn.") == std::string::npos ||
      labeled.markdown.find("02.900] · Speaker 1 + Speaker 2**") == std::string::npos ||
      labeled.markdown.find("04] · Speaker 2**") == std::string::npos ||
      labeled.markdown.find("11.000] · Unknown speaker**") == std::string::npos) {
    std::cerr << "Speaker labels did not preserve order, overlap, or original text\n";
    return 1;
  }

  const std::filesystem::path path(argv[1]);
  tinto::WavWriter writer;
  const float first[]{0.125f, 0.25f, 0.5f};
  const float second[]{0.625f, 0.75f};
  if (!writer.Open(path.string()) || !writer.WriteAligned(2, first, 3) ||
      !writer.WriteAligned(4, second, 2) || !writer.WriteAligned(8, first, 1)) {
    std::cerr << "Could not write aligned PCM\n";
    return 1;
  }
  writer.Close();
  const auto pcm = tinto::ReadMonoWav(path);
  std::filesystem::remove(path);
  const std::vector<float> expected{0, 0, 0.125f, 0.25f, 0.5f, 0.75f, 0, 0, 0.125f};
  if (pcm != expected) {
    std::cerr << "Aligned WAV did not preserve gaps or trim duplicated audio\n";
    return 1;
  }
}
