#include <chrono>
#include <iostream>
#include <vector>

#include "tinto/audio_mixer.hpp"

int main() {
  using namespace std::chrono_literals;
  using tinto::AudioSource;
  constexpr auto rate = tinto::kCaptureSampleRate;
  const auto epoch = tinto::AudioMixer::Clock::time_point{};
  tinto::AudioMixer mixer;
  mixer.Reset(epoch);

  const float input[] = {0.75f, 0.5f, -0.25f};
  const float system[] = {0.5f, -0.5f, 0.5f};
  // Deliver the later packet first to simulate independent callback threads.
  mixer.Push({system, 3, rate, 1, AudioSource::kSystem, epoch + 312500ns});
  mixer.Push({input, 3, rate, 1, AudioSource::kInput, epoch + 187500ns});
  if (!mixer.Drain(epoch + 1ms, false).empty()) {
    std::cerr << "Mixer did not wait for a late callback\n";
    return 1;
  }
  if (mixer.Drain(epoch + 1s, false) !=
      std::vector<float>{0.75f, 0.5f, 0.25f, -0.5f, 0.5f}) {
    std::cerr << "Expected a mixed timeline, not concatenated device buffers\n";
    return 1;
  }
  if (!mixer.Drain(epoch + 1s, true).empty()) {
    std::cerr << "Final drain duplicated already processed samples\n";
    return 1;
  }

  mixer.Reset(epoch);
  mixer.Push({input, 3, rate, 1, AudioSource::kInput, epoch + 312500ns});
  mixer.Push({input, 3, rate, 1, AudioSource::kSystem, epoch + 312500ns});
  if (mixer.Drain(epoch + 1ms, true) !=
      std::vector<float>{0, 0, 1.0f, 1.0f, -0.5f}) {
    std::cerr << "Final drain did not preserve gaps, sum, and clamp the mix\n";
    return 1;
  }

  mixer.Reset(epoch);
  mixer.Push({input, 3, rate, 1, AudioSource::kInput, epoch + 50s});
  const auto bounded = mixer.Drain(epoch + 50s, true);
  if (bounded.size() != 30 * rate || bounded.back() != input[2]) {
    std::cerr << "Mixer history exceeded the ring or lost the newest samples\n";
    return 1;
  }
}
