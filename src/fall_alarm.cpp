#include "fall_alarm.hpp"

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

FallAlarm::FallAlarm(std::string sound_path, size_t window_size,
                     size_t required_votes, double cooldown_seconds)
    : sound_path_(std::move(sound_path)), window_size_(window_size),
      required_votes_(required_votes), cooldown_seconds_(cooldown_seconds) {
  if (window_size_ == 0 || required_votes_ == 0 ||
      required_votes_ > window_size_ || cooldown_seconds_ < 0)
    throw std::invalid_argument("invalid fall alarm queue configuration");
}

FallAlarm::~FallAlarm() { reap_player(); }

bool FallAlarm::update(bool fall_detected, double now) {
  reap_player();
  const unsigned char result = fall_detected ? 1 : 0;
  results_.push_back(result);
  votes_ += result;
  if (results_.size() > window_size_) {
    votes_ -= results_.front();
    results_.pop_front();
  }

  const bool enough_history = results_.size() == window_size_;
  const bool should_alarm = enough_history && votes_ >= required_votes_;
  if (!should_alarm) {
    latched_ = false;
    return false;
  }
  if (latched_ || now - last_played_ < cooldown_seconds_)
    return false;

  latched_ = true;
  if (!play())
    return false;
  last_played_ = now;
  std::cerr << "[SPEAKER_ALERT] votes=" << votes_ << '/' << window_size_
            << " sound=" << sound_path_ << '\n';
  return true;
}

void FallAlarm::reap_player() {
  if (player_pid_ <= 0)
    return;
  const pid_t result = waitpid(player_pid_, nullptr, WNOHANG);
  if (result == player_pid_ || (result < 0 && errno == ECHILD))
    player_pid_ = -1;
}

bool FallAlarm::play() {
  if (access(sound_path_.c_str(), R_OK) != 0) {
    std::cerr << "[SPEAKER_ALERT] cannot read " << sound_path_ << '\n';
    return false;
  }
  if (player_pid_ > 0)
    return false;
  const pid_t child = fork();
  if (child < 0) {
    std::cerr << "[SPEAKER_ALERT] fork failed errno=" << errno << '\n';
    return false;
  }
  if (child == 0) {
    execl("/usr/bin/aplay", "aplay", "-q", sound_path_.c_str(),
          static_cast<char *>(nullptr));
    _exit(127);
  }
  player_pid_ = child;
  return true;
}
