#include "frame_overlay.hpp"
#include <algorithm>
#include <array>
#include <iomanip>
#include <map>
#include <sstream>

namespace {
void pixel(std::vector<uint8_t> &image, int x, int y, uint8_t r, uint8_t g,
           uint8_t b) {
  if (x < 0 || y < 0 || x >= kInputSize || y >= kInputSize)
    return;
  const size_t offset = (static_cast<size_t>(y) * kInputSize + x) * 3;
  image[offset] = r;
  image[offset + 1] = g;
  image[offset + 2] = b;
}
void rect(std::vector<uint8_t> &image, int x1, int y1, int x2, int y2,
          uint8_t r, uint8_t g, uint8_t b) {
  x1 = std::clamp(x1, 0, kInputSize);
  y1 = std::clamp(y1, 0, kInputSize);
  x2 = std::clamp(x2, 0, kInputSize);
  y2 = std::clamp(y2, 0, kInputSize);
  for (int y = y1; y < y2; ++y)
    for (int x = x1; x < x2; ++x)
      pixel(image, x, y, r, g, b);
}
void line(std::vector<uint8_t> &image, int x0, int y0, int x1, int y1,
          uint8_t r, uint8_t g, uint8_t b, int thickness = 2) {
  const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1,
            dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    for (int yy = -thickness; yy <= thickness; ++yy)
      for (int xx = -thickness; xx <= thickness; ++xx)
        pixel(image, x0 + xx, y0 + yy, r, g, b);
    if (x0 == x1 && y0 == y1)
      break;
    const int twice = 2 * error;
    if (twice >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twice <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}
const std::array<uint8_t, 7> &glyph(char c) {
  static const std::map<char, std::array<uint8_t, 7>> font = {
      {' ', {0, 0, 0, 0, 0, 0, 0}},        {'-', {0, 0, 0, 31, 0, 0, 0}},
      {'_', {0, 0, 0, 0, 0, 0, 31}},       {'.', {0, 0, 0, 0, 0, 12, 12}},
      {':', {0, 12, 12, 0, 12, 12, 0}},    {'0', {14, 17, 19, 21, 25, 17, 14}},
      {'1', {4, 12, 4, 4, 4, 4, 14}},      {'2', {14, 17, 1, 2, 4, 8, 31}},
      {'3', {30, 1, 1, 14, 1, 1, 30}},     {'4', {2, 6, 10, 18, 31, 2, 2}},
      {'5', {31, 16, 16, 30, 1, 1, 30}},   {'6', {14, 16, 16, 30, 17, 17, 14}},
      {'7', {31, 1, 2, 4, 8, 8, 8}},       {'8', {14, 17, 17, 14, 17, 17, 14}},
      {'9', {14, 17, 17, 15, 1, 1, 14}},   {'A', {14, 17, 17, 31, 17, 17, 17}},
      {'B', {30, 17, 17, 30, 17, 17, 30}}, {'C', {14, 17, 16, 16, 16, 17, 14}},
      {'D', {30, 17, 17, 17, 17, 17, 30}}, {'E', {31, 16, 16, 30, 16, 16, 31}},
      {'F', {31, 16, 16, 30, 16, 16, 16}}, {'G', {14, 17, 16, 23, 17, 17, 15}},
      {'H', {17, 17, 17, 31, 17, 17, 17}}, {'I', {14, 4, 4, 4, 4, 4, 14}},
      {'J', {7, 2, 2, 2, 2, 18, 12}},      {'K', {17, 18, 20, 24, 20, 18, 17}},
      {'L', {16, 16, 16, 16, 16, 16, 31}}, {'M', {17, 27, 21, 21, 17, 17, 17}},
      {'N', {17, 25, 21, 19, 17, 17, 17}}, {'O', {14, 17, 17, 17, 17, 17, 14}},
      {'P', {30, 17, 17, 30, 16, 16, 16}}, {'Q', {14, 17, 17, 17, 21, 18, 13}},
      {'R', {30, 17, 17, 30, 20, 18, 17}}, {'S', {15, 16, 16, 14, 1, 1, 30}},
      {'T', {31, 4, 4, 4, 4, 4, 4}},       {'U', {17, 17, 17, 17, 17, 17, 14}},
      {'V', {17, 17, 17, 17, 17, 10, 4}},  {'W', {17, 17, 17, 21, 21, 21, 10}},
      {'X', {17, 17, 10, 4, 10, 17, 17}},  {'Y', {17, 17, 10, 4, 4, 4, 4}},
      {'Z', {31, 1, 2, 4, 8, 16, 31}}};
  static const std::array<uint8_t, 7> unknown = {31, 17, 2, 4, 4, 0, 4};
  const auto it = font.find(c);
  return it == font.end() ? unknown : it->second;
}
void text(std::vector<uint8_t> &image, int x, int y, const std::string &value,
          uint8_t r, uint8_t g, uint8_t b, int scale = 2) {
  for (char c : value) {
    const auto &rows = glyph(c);
    for (int yy = 0; yy < 7; ++yy)
      for (int xx = 0; xx < 5; ++xx)
        if (rows[yy] & (1 << (4 - xx)))
          rect(image, x + xx * scale, y + yy * scale, x + (xx + 1) * scale,
               y + (yy + 1) * scale, r, g, b);
    x += 6 * scale;
  }
}
} // namespace

void annotate_frame(std::vector<uint8_t> &image, const std::vector<Pose> &poses,
                    const std::vector<PoseOverlay> &overlays, float fps) {
  static constexpr int bones[][2] = {{5, 6},   {5, 7},   {7, 9},   {6, 8},
                                     {8, 10},  {5, 11},  {6, 12},  {11, 12},
                                     {11, 13}, {13, 15}, {12, 14}, {14, 16}};
  rect(image, 0, 0, kInputSize, 28, 0, 0, 0);
  std::ostringstream hud;
  hud << "FPS:" << std::fixed << std::setprecision(1) << fps
      << " PEOPLE:" << poses.size() << " INT8";
  text(image, 8, 7, hud.str(), 30, 220, 255, 2);
  for (size_t i = 0; i < poses.size(); ++i) {
    const Pose &pose = poses[i];
    const PoseOverlay overlay =
        i < overlays.size() ? overlays[i] : PoseOverlay{};
    const bool danger = overlay.state != "MONITORING";
    const uint8_t r = danger ? 255 : 30, g = danger ? 40 : 240, b = 40;
    const int x1 = std::clamp(static_cast<int>(pose.x1), 0, kInputSize - 1),
              y1 = std::clamp(static_cast<int>(pose.y1), 0, kInputSize - 1),
              x2 = std::clamp(static_cast<int>(pose.x2), 0, kInputSize - 1),
              y2 = std::clamp(static_cast<int>(pose.y2), 0, kInputSize - 1);
    line(image, x1, y1, x2, y1, r, g, b);
    line(image, x2, y1, x2, y2, r, g, b);
    line(image, x2, y2, x1, y2, r, g, b);
    line(image, x1, y2, x1, y1, r, g, b);
    for (const auto &bone : bones) {
      const auto &a = pose.keypoints[bone[0]];
      const auto &c = pose.keypoints[bone[1]];
      if (a.confidence > 0.25F && c.confidence > 0.25F)
        line(image, a.x, a.y, c.x, c.y, 30, 180, 255, 1);
    }
    for (const auto &point : pose.keypoints)
      if (point.confidence > 0.25F)
        rect(image, point.x - 2, point.y - 2, point.x + 3, point.y + 3, 255,
             220, 20);
    std::ostringstream first, second;
    first << "ID:" << overlay.track_id << " CONF:" << std::fixed
          << std::setprecision(2) << pose.confidence;
    second << "ANGLE:" << std::fixed << std::setprecision(1) << overlay.angle
           << " " << overlay.state;
    const int label_x = std::clamp(x1, 0, kInputSize - 250),
              label_y = std::max(31, y1 - 36),
              width = static_cast<int>(
                  std::max(first.str().size(), second.str().size()) * 6 + 6);
    rect(image, label_x, label_y, std::min(kInputSize, label_x + width),
         label_y + 32, 0, 0, 0);
    text(image, label_x + 3, label_y + 2, first.str(), r, g, b, 1);
    text(image, label_x + 3, label_y + 17, second.str(), r, g, b, 1);
  }
}
