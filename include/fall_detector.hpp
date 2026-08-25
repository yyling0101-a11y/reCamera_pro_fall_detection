#pragma once

#include "pose_types.hpp"
#include <map>
#include <vector>

class WebSocketServer;
class FallDetector {
public:
  FallDetector(float inactivity, WebSocketServer &websocket);
  std::vector<PoseOverlay> update(const std::vector<Pose> &poses, double now);

private:
  struct Track {
    int id{};
    Pose pose{}, previous{};
    bool has_previous = false;
    double previous_time = 0;
    std::string state = "MONITORING";
    double fall_started = 0, last_seen = 0, last_alert = 0;
    float angle = 0, velocity = 0;
  };
  void step(Track &track, const Pose &pose, double now);
  void alert(Track &track, float angle, float velocity, double now);
  float inactivity_;
  WebSocketServer &websocket_;
  std::map<int, Track> tracks_;
  int next_id_ = 1;
};
