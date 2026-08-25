#include "fall_detector.hpp"
#include "websocket_server.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

FallDetector::FallDetector(float inactivity, WebSocketServer &websocket)
    : inactivity_(inactivity), websocket_(websocket) {}
std::vector<PoseOverlay> FallDetector::update(const std::vector<Pose> &poses,
                                              double now) {
  std::vector<PoseOverlay> overlays(poses.size());
  std::vector<int> assigned(poses.size(), 0);
  for (auto &[id, track] : tracks_) {
    float best = 0;
    int index = -1;
    for (size_t i = 0; i < poses.size(); ++i)
      if (!assigned[i]) {
        const float overlap = pose_iou(track.pose, poses[i]);
        if (overlap > best) {
          best = overlap;
          index = static_cast<int>(i);
        }
      }
    if (index >= 0 && best >= 0.20F) {
      assigned[index] = 1;
      step(track, poses[index], now);
      overlays[index] = {track.id, track.state, track.angle, track.velocity};
    }
  }
  for (size_t i = 0; i < poses.size(); ++i)
    if (!assigned[i]) {
      Track track;
      track.id = next_id_++;
      track.pose = poses[i];
      track.last_seen = now;
      track.previous_time = now;
      track.angle = torso_angle(poses[i]);
      tracks_[track.id] = track;
      overlays[i] = {track.id, track.state, track.angle, track.velocity};
    }
  for (auto it = tracks_.begin(); it != tracks_.end();)
    if (now - it->second.last_seen > 1.0)
      it = tracks_.erase(it);
    else
      ++it;
  return overlays;
}
void FallDetector::step(Track &track, const Pose &pose, double now) {
  track.previous = track.pose;
  track.pose = pose;
  track.last_seen = now;
  const float angle = torso_angle(pose),
              aspect = (pose.x2 - pose.x1) / std::max(1.0F, pose.y2 - pose.y1);
  float velocity = 0, angular = 0;
  if (track.has_previous) {
    const float dt =
        std::max(0.01F, static_cast<float>(now - track.previous_time));
    const float height = std::max(50.0F, pose.y2 - pose.y1);
    velocity = (torso_y(pose) - torso_y(track.previous)) / (dt * height);
    angular = (torso_angle(track.previous) - angle) / dt;
  }
  track.has_previous = true;
  track.previous_time = now;
  const bool dynamic = velocity > 1.2F || (angular > 35 && angle < 55) ||
                       (velocity > 0.6F && angle < 55);
  if (dynamic) {
    if (track.state == "MONITORING")
      track.fall_started = now;
    track.state = "FALL_DETECTED";
  } else if (track.state == "FALL_DETECTED") {
    if ((angle < 55 || aspect > 0.85F) &&
        now - track.fall_started >= inactivity_) {
      track.state = "CONFIRMED_FALL";
      alert(track, angle, velocity, now);
    } else if (angle > 70 && velocity < 0)
      track.state = "MONITORING";
  } else if (track.state == "CONFIRMED_FALL" && angle > 70)
    track.state = "MONITORING";
  track.angle = angle;
  track.velocity = velocity;
  std::cout << "track=" << track.id << " state=" << track.state
            << " conf=" << std::fixed << std::setprecision(2) << pose.confidence
            << " angle=" << angle << " velocity=" << velocity << '\n';
}
void FallDetector::alert(Track &track, float angle, float velocity,
                         double now) {
  if (now - track.last_alert < 10)
    return;
  track.last_alert = now;
  const auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  std::ostringstream json;
  json << "{\"type\":\"fall_alert\",\"severity\":\"critical\",\"track_id\":"
       << track.id << ",\"state\":\"CONFIRMED_FALL\",\"timestamp_ms\":" << epoch
       << ",\"torso_angle\":" << std::fixed << std::setprecision(2) << angle
       << ",\"vertical_velocity\":" << velocity << "}";
  const size_t sent = websocket_.broadcast_text(json.str());
  std::cerr << "[ALERT] " << json.str() << " websocket_clients_sent=" << sent
            << '\n';
}
