#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <sys/types.h>

class FallAlarm {
public:
  FallAlarm(std::string sound_path, size_t window_size, size_t required_votes,
            double cooldown_seconds);
  ~FallAlarm();
  bool update(bool fall_detected, double now);

private:
  void reap_player();
  bool play();

  std::string sound_path_;
  size_t window_size_;
  size_t required_votes_;
  double cooldown_seconds_;
  double last_played_ = -1e9;
  size_t votes_ = 0;
  bool latched_ = false;
  pid_t player_pid_ = -1;
  std::deque<unsigned char> results_;
};
