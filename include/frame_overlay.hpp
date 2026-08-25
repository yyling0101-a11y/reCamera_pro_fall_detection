#pragma once

#include "pose_types.hpp"
#include <cstdint>
#include <vector>

void annotate_frame(std::vector<uint8_t> &image, const std::vector<Pose> &poses,
                    const std::vector<PoseOverlay> &overlays, float fps);
