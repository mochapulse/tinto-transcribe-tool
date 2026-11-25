#include "diarize.hpp"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "tinto/capture.hpp"
#include "tinto/diarization.hpp"
#include "tinto/paths.hpp"
#include "tinto/storage.hpp"

namespace {

namespace fs = std::filesystem;

#ifdef TINTO_WITH_DIARIZATION
struct Options {
  fs::path transcript;
  fs::path audio;
  fs::path output;
  int threads = 2;
  int speakers = 0;
};

int PositiveInteger(const std::string& value, const std::string& flag) {
  int number = 0;
  const auto* end = value.data() + value.size();
  const auto parsed = std::from_chars(value.data(), end, number);
  if (parsed.ec != std::errc{} || parsed.ptr != end || number < 1) {
    throw std::invalid_argument(flag + " requires a positive integer");
  }
  return number;
}

Options Parse(const std::vector<std::string>& args) {
  Options opts;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const auto& arg = args[i];
    if (!arg.empty() && arg[0] != '-') {
      if (!opts.transcript.empty()) throw std::invalid_argument("expected one transcript path");
      opts.transcript = arg;
    } else if (arg == "--audio" || arg == "-a" || arg == "--out" || arg == "-o" ||
               arg == "--threads" || arg == "-t" || arg == "--speakers" || arg == "-s") {
      if (++i == args.size()) throw std::invalid_argument("missing value for " + arg);
      const auto& value = args[i];
      if (arg == "--audio" || arg == "-a") opts.audio = value;
      if (arg == "--out" || arg == "-o") opts.output = value;
      if (arg == "--threads" || arg == "-t") opts.threads = PositiveInteger(value, arg);
      if (arg == "--speakers" || arg == "-s") opts.speakers = PositiveInteger(value, arg);
    } else {
      throw std::invalid_argument("unknown option: " + arg);
    }
  }
  if (!opts.audio.empty() && opts.transcript.empty()) {
    throw std::invalid_argument("--audio requires a transcript path");
  }
  return opts;
}

fs::path PairedAudio(const fs::path& transcript) {
  auto name = transcript.filename();
  name.replace_extension(".aligned.wav");
  for (const auto& dir : {transcript.parent_path(), tinto::paths::recordings_dir()}) {
    const auto file = dir / name;
    if (fs::is_regular_file(file)) return file;
  }
  return {};
}

fs::path LatestSession() {
  const auto folder = tinto::paths::transcripts_dir();
  fs::path latest;
  if (!fs::is_directory(folder)) return latest;
  for (const auto& entry : fs::directory_iterator(folder)) {
    const auto& path = entry.path();
    if (entry.is_regular_file() && path.extension() == ".md" && path > latest &&
        !PairedAudio(path).empty()) {
      latest = path;
    }
  }
  return latest;
}

std::string ReadFinalTranscript(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open transcript: " + path.string());
  std::ostringstream text;
  text << input.rdbuf();
  auto markdown = text.str();
  if (markdown.find("\n*Recording ended.") == std::string::npos) {
    throw std::runtime_error("transcript is not finalized; stop the recording first");
  }
  return markdown;
}

void Publish(const fs::path& output, const std::string& markdown) {
  auto staging = output;
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  staging += "." + std::to_string(nonce) + ".tmp";
  try {
    std::ofstream file(staging, std::ios::binary);
    file << markdown;
    file.close();
    if (!file) throw std::runtime_error("cannot write speaker transcript: " + output.string());
    if (fs::exists(output)) throw std::runtime_error("output exists; choose another --out path");
    fs::rename(staging, output);
  } catch (...) {
    std::error_code ignored;
    fs::remove(staging, ignored);
    throw;
  }
}
#endif

}  // namespace

int CmdDiarize(const std::vector<std::string>& args) {
#ifndef TINTO_WITH_DIARIZATION
  (void)args;
  std::cerr << "diarize: this build has no speaker diarization support\n";
  return 1;
#else
  try {
    auto opts = Parse(args);
    if (opts.transcript.empty()) opts.transcript = LatestSession();
    if (opts.transcript.empty()) {
      throw std::runtime_error("no eligible session; record with --mode 2 --diarize first");
    }
    if (opts.audio.empty()) opts.audio = PairedAudio(opts.transcript);
    if (opts.audio.empty()) {
      throw std::runtime_error("no aligned WAV; use a session recorded with --diarize or --audio WAV");
    }
    if (opts.output.empty()) opts.output = fs::path(opts.transcript).replace_extension(".speakers.md");
    if (fs::exists(opts.output)) throw std::runtime_error("output exists; choose another --out path");

    const auto markdown = ReadFinalTranscript(opts.transcript);
    const auto samples = tinto::ReadMonoWav(opts.audio);
    std::cout << "diarize: analyzing " << opts.audio << " on CPU" << std::endl;
    const auto started = std::chrono::steady_clock::now();
    auto turns = tinto::AnalyzeSpeakers(samples, tinto::paths::models_dir() / "diarization",
                                        opts.threads, opts.speakers);
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    auto labeled = tinto::LabelTranscript(markdown, std::move(turns));
    if (labeled.labeled_segments == 0) {
      throw std::runtime_error("no speaker turns matched the transcript timestamps");
    }
    Publish(opts.output, labeled.markdown);
    const auto duration = static_cast<double>(samples.size()) / tinto::kCaptureSampleRate;
    std::cout << "diarize: " << labeled.speakers << " speaker(s), " << labeled.labeled_segments
              << " labeled segment(s), " << std::fixed << std::setprecision(2) << elapsed.count()
              << " s for " << duration << " s of audio (RTF " << elapsed.count() / duration << ")\n"
              << "diarize: speaker transcript saved to " << opts.output << "\n";
    return 0;
  } catch (const std::invalid_argument& e) {
    std::cerr << "diarize: " << e.what() << "\n";
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "diarize: " << e.what() << "\n";
    return 1;
  }
#endif
}
