#include "help.hpp"

#include <ostream>

#include "tinto/version.hpp"

int PrintHelp(std::ostream& out, std::string_view command) {
  if (command.empty()) {
    out << "tinto " << tinto::kVersion << " — local meeting recording and transcription\n\n"
        << "Usage: tinto <command> [options]\n\n"
        << "Commands:\n"
        << "  init      Profile this machine and download all required local models\n"
        << "  record    Capture audio, transcribe it, or both\n"
        << "  status    Show whether the recording worker is running\n"
        << "  stop      Finish a recording and close its files\n"
        << "  diarize   Label speakers in a finished recording\n"
        << "  health    Check the profile, model cache, and audio availability\n"
        << "  doctor    Show diagnostics and verify every model checksum\n"
        << "  help      Show help for a command\n\n"
        << "  -h, --help     Show help\n"
        << "  -v, --version  Show version (-V also works)\n\n"
        << "Examples:\n"
        << "  tinto init\n"
        << "  tinto record -f -m 3 -s system -q medium -l es\n"
        << "  tinto record -m 2 -D\n"
        << "  tinto stop\n"
        << "  tinto diarize\n\n"
        << "Run 'tinto help record' for flags and modes.\n"
        << "Data lives in ~/.tinto unless TINTO_HOME is set.\n";
  } else if (command == "init") {
    out << "Usage: tinto init [-n|--skip-models]\n\n"
        << "Write the machine profile and verify or download all models used by this build.\n"
        << "  -n, --skip-models  Write the profile without downloading or verifying models\n"
        << "  -h, --help         Show this help\n\n"
        << "Example: tinto init -n  # prepare directories on an offline machine\n";
  } else if (command == "record") {
    out << "Usage: tinto record [options]\n\n"
        << "  -f, --foreground     Stay in this console. Ctrl+C stops capture and finalizes.\n"
        << "  -m, --mode MODE      1: WAV only, 2: WAV and Markdown, 3: Markdown only\n"
        << "                       Names: record-only, record+transcribe, transcribe-only.\n"
        << "  -s, --source SOURCE  input, system, or both. Default: available safe sources.\n"
        << "  -d, --device NAME    Choose a microphone by part of its name.\n"
        << "                       Records only that input unless used with --source both.\n"
        << "  -q, --quality MODEL  base, small, medium, or turbo (Whisper model).\n"
        << "  -l, --lang CODE      Transcription language, e.g. es or en.\n"
        << "  -t, --threads N      Positive number of transcription threads.\n"
        << "  -o, --out DIRECTORY  Override the directory for audio and Markdown.\n"
        << "  -D, --diarize        Mode 2 only. Keep a time-aligned WAV for 'tinto diarize'.\n"
        << "  -h, --help           Show this help\n\n"
        << "Default mode is 2. Flags override the machine profile.\n"
        << "Without -f, record runs in the background. Use 'tinto stop' when finished.\n"
        << "In the foreground, a second Ctrl+C skips pending transcription.\n\n"
        << "Examples:\n"
        << "  tinto record -f -m 1 -d \"Microphone Array\"\n"
        << "  tinto record -m 2 -s both -D\n"
        << "  tinto record -f -m 3 -s system -q medium -l es\n";
  } else if (command == "diarize") {
    out << "Usage: tinto diarize [transcript.md] [options]\n\n"
        << "Label the latest eligible session or the specified transcript.\n"
        << "Creates a separate .speakers.md. Both inputs stay unchanged.\n"
        << "  -a, --audio FILE  Use a WAV on the same timeline as the supplied transcript\n"
        << "  -s, --speakers N  Use a known speaker count (default: automatic)\n"
        << "  -t, --threads N   CPU inference threads (default: 2)\n"
        << "  -o, --out FILE    Write to a new file instead of the default .speakers.md\n"
        << "  -h, --help        Show this help\n\n"
        << "Example: tinto diarize -s 2 -o meeting-with-speakers.md\n";
  } else if (command == "status" || command == "stop" || command == "health" ||
             command == "doctor" || command == "help") {
    out << "Usage: tinto " << command << (command == "help" ? " [command]" : " [-h|--help]")
        << "\n\n";
    if (command == "status") out << "Report the recording worker. Exit 1 when idle.\n";
    if (command == "stop") out << "Request orderly shutdown and finalize the recording.\n";
    if (command == "health") out << "Check the profile and model sizes without opening an audio stream.\n";
    if (command == "doctor") out << "Show paths, audio endpoints, and SHA-256 verification for every model.\n";
    if (command == "help") out << "Example: tinto help record\n";
  } else {
    out << "tinto: unknown command '" << command << "'\n";
    return 2;
  }
  return 0;
}
