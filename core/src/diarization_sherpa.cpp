#include "tinto/diarization.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

#include <sherpa-onnx-c-api.h>

namespace tinto {

std::vector<SpeakerTurn> AnalyzeSpeakers(const std::vector<float>& samples,
                                         const std::filesystem::path& models,
                                         int threads, int speaker_count) {
  if (samples.empty() || samples.size() > std::numeric_limits<std::int32_t>::max()) {
    throw std::runtime_error("empty or unsupported audio length for diarization");
  }

  const auto segmentation = models / "segmentation.int8.onnx";
  const auto embedding = models / "embedding.onnx";
  for (const auto& path : {segmentation, embedding}) {
    if (!std::filesystem::is_regular_file(path)) {
      throw std::runtime_error("missing model: " + path.string());
    }
  }
  const auto segmentation_path = segmentation.u8string();
  const auto embedding_path = embedding.u8string();

  SherpaOnnxOfflineSpeakerDiarizationConfig cfg{};
  cfg.segmentation.pyannote.model = segmentation_path.c_str();
  cfg.segmentation.provider = "cpu";
  cfg.segmentation.num_threads = threads;
  cfg.embedding.model = embedding_path.c_str();
  cfg.embedding.provider = "cpu";
  cfg.embedding.num_threads = threads;
  cfg.clustering.num_clusters = speaker_count == 0 ? -1 : speaker_count;
  cfg.clustering.threshold = 0.5f;
  cfg.min_duration_on = 0.3f;
  cfg.min_duration_off = 0.5f;

  const std::unique_ptr<const SherpaOnnxOfflineSpeakerDiarization,
                        decltype(&SherpaOnnxDestroyOfflineSpeakerDiarization)> diarizer(
      SherpaOnnxCreateOfflineSpeakerDiarization(&cfg), SherpaOnnxDestroyOfflineSpeakerDiarization);
  if (!diarizer) throw std::runtime_error("could not initialize the speaker models");

  const std::unique_ptr<const SherpaOnnxOfflineSpeakerDiarizationResult,
                        decltype(&SherpaOnnxOfflineSpeakerDiarizationDestroyResult)> result(
      SherpaOnnxOfflineSpeakerDiarizationProcess(diarizer.get(), samples.data(),
                                                 static_cast<std::int32_t>(samples.size())),
      SherpaOnnxOfflineSpeakerDiarizationDestroyResult);
  if (!result) throw std::runtime_error("speaker diarization failed");
  const auto count = SherpaOnnxOfflineSpeakerDiarizationResultGetNumSegments(result.get());
  if (count <= 0) return {};

  const std::unique_ptr<const SherpaOnnxOfflineSpeakerDiarizationSegment[],
                        decltype(&SherpaOnnxOfflineSpeakerDiarizationDestroySegment)> segments(
      SherpaOnnxOfflineSpeakerDiarizationResultSortByStartTime(result.get()),
      SherpaOnnxOfflineSpeakerDiarizationDestroySegment);
  if (!segments) throw std::runtime_error("could not read speaker turns");
  std::vector<SpeakerTurn> turns;
  turns.reserve(static_cast<std::size_t>(count));
  for (std::int32_t i = 0; i < count; ++i) {
    turns.push_back({segments[i].start, segments[i].end, segments[i].speaker});
  }
  return turns;
}

}  // namespace tinto
