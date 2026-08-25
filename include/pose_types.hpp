#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

constexpr int kInputSize = 640;
constexpr int kKeypoints = 17;

struct Keypoint {
  float x{}, y{}, confidence{};
};

struct Pose {
  float x1{}, y1{}, x2{}, y2{}, confidence{};
  std::array<Keypoint, kKeypoints> keypoints{};
};

struct PoseOverlay {
  int track_id = -1;
  std::string state = "MONITORING";
  float angle = 0;
  float velocity = 0;
};

inline float pose_iou(const Pose &a, const Pose &b) {
  const float left = std::max(a.x1, b.x1), top = std::max(a.y1, b.y1);
  const float right = std::min(a.x2, b.x2), bottom = std::min(a.y2, b.y2);
  const float intersection =
      std::max(0.0F, right - left) * std::max(0.0F, bottom - top);
  const float area_a =
      std::max(0.0F, a.x2 - a.x1) * std::max(0.0F, a.y2 - a.y1);
  const float area_b =
      std::max(0.0F, b.x2 - b.x1) * std::max(0.0F, b.y2 - b.y1);
  return intersection / (area_a + area_b - intersection + 1e-6F);
}

inline float torso_angle(const Pose &pose) {
  const float sx = (pose.keypoints[5].x + pose.keypoints[6].x) * 0.5F;
  const float sy = (pose.keypoints[5].y + pose.keypoints[6].y) * 0.5F;
  const float hx = (pose.keypoints[11].x + pose.keypoints[12].x) * 0.5F;
  const float hy = (pose.keypoints[11].y + pose.keypoints[12].y) * 0.5F;
  return std::atan2(std::abs(sy - hy), std::abs(sx - hx) + 1e-6F) * 180.0F /
         3.14159265358979323846F;
}

inline float torso_y(const Pose &pose) {
  return (pose.keypoints[5].y + pose.keypoints[6].y + pose.keypoints[11].y +
          pose.keypoints[12].y) *
         0.25F;
}
