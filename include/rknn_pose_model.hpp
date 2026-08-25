#pragma once

#include "pose_types.hpp"
#include <cstddef>
#include <cstdint>
#include <rknn_api.h>
#include <string>
#include <vector>

class RknnPoseModel {
public:
  explicit RknnPoseModel(const std::string &path);
  ~RknnPoseModel();
  RknnPoseModel(const RknnPoseModel &) = delete;
  RknnPoseModel &operator=(const RknnPoseModel &) = delete;
  std::vector<Pose> infer(const uint8_t *rgb, size_t size, float confidence,
                          float nms);

private:
  rknn_context context_ = 0;
  float confidence_output_scale_ = 1.0F;
  std::vector<uint8_t> model_data_;
};
