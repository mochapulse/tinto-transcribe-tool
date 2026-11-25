#include "tinto/diarization.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <regex>
#include <sstream>

namespace tinto {
namespace {

double Seconds(const std::string& text) {
  const auto first = text.find(':');
  const auto second = first + 3;
  const int h = std::stoi(text.substr(0, first));
  const int m = std::stoi(text.substr(first + 1, 2));
  const int s = std::stoi(text.substr(second + 1, 2));
  const int ms = text.size() > second + 3 ? std::stoi(text.substr(second + 4)) : 0;
  return h * 3600.0 + m * 60 + s + ms / 1000.0;
}

}  // namespace

LabeledTranscript LabelTranscript(const std::string& markdown, std::vector<SpeakerTurn> turns) {
  turns.erase(std::remove_if(turns.begin(), turns.end(), [](const SpeakerTurn& turn) {
    return !std::isfinite(turn.start_sec) || !std::isfinite(turn.end_sec) ||
           turn.start_sec < 0 || turn.end_sec <= turn.start_sec || turn.cluster < 0;
  }), turns.end());
  std::stable_sort(turns.begin(), turns.end(), [](const auto& a, const auto& b) {
    return a.start_sec < b.start_sec;
  });
  std::map<int, std::size_t> numbers;
  for (const auto& turn : turns) {
    numbers.try_emplace(turn.cluster, numbers.size() + 1);
  }

  const std::regex heading(
      R"(^\*\*\[([0-9]{2,6}:[0-5][0-9]:[0-5][0-9](?:\.[0-9]{3})?) — )"
      R"(([0-9]{2,6}:[0-5][0-9]:[0-5][0-9](?:\.[0-9]{3})?)\]\*\*$)");
  std::istringstream input(markdown);
  std::ostringstream output;
  LabeledTranscript result;
  result.speakers = numbers.size();

  for (std::string line; std::getline(input, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::smatch match;
    if (std::regex_match(line, match, heading)) {
      const auto begin = Seconds(match[1].str());
      const auto end = Seconds(match[2].str());
      std::map<std::size_t, double> coverage;
      for (const auto& turn : turns) {
        if (turn.start_sec >= end) break;
        const auto seconds = std::min(end, turn.end_sec) - std::max(begin, turn.start_sec);
        if (seconds > 0) coverage[numbers.at(turn.cluster)] += seconds;
      }
      double primary = 0;
      for (const auto& item : coverage) primary = std::max(primary, item.second);
      const auto minimum = std::max(primary * 0.25, std::min(0.15, (end - begin) / 2));
      std::string label;
      for (const auto& item : coverage) {
        if (item.second < minimum) continue;
        if (!label.empty()) label += " + ";
        label += "Speaker " + std::to_string(item.first);
      }
      if (!label.empty()) ++result.labeled_segments;
      line.insert(line.size() - 2, " · " + (label.empty() ? "Unknown speaker" : label));
    }
    output << line << '\n';
  }
  result.markdown = output.str();
  return result;
}

}  // namespace tinto
