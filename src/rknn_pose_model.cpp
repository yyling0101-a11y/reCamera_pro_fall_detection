#include "rknn_pose_model.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
constexpr int kChannels = 56, kCandidates = 8400;
void require_rknn(int code, const char *operation) {
  if (code != RKNN_SUCC)
    throw std::runtime_error(std::string(operation) +
                             " failed: " + std::to_string(code));
}
std::vector<uint8_t> read_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input)
    throw std::runtime_error("cannot open model: " + path);
  const auto size = input.tellg();
  if (size <= 0)
    throw std::runtime_error("empty model: " + path);
  std::vector<uint8_t> data(static_cast<size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(data.data()), size);
  if (!input)
    throw std::runtime_error("cannot read model: " + path);
  return data;
}

std::vector<Pose> decode(const float *output, float confidence, float nms,
                         float confidence_output_scale) {
  auto at = [&](int channel, int candidate) {
    return output[channel * kCandidates + candidate];
  };
  std::vector<Pose> poses;
  for (int i = 0; i < kCandidates; ++i) {
    const float score = at(4, i) / confidence_output_scale;

    if (score < confidence)
      continue;
    Pose p;
    const float cx = at(0, i), cy = at(1, i), w = at(2, i), h = at(3, i);
    p.x1 = cx - w * 0.5F;
    p.y1 = cy - h * 0.5F;
    p.x2 = cx + w * 0.5F;
    p.y2 = cy + h * 0.5F;
    p.confidence = score;
    int visible = 0;
    for (int k = 0; k < kKeypoints; ++k) {
      p.keypoints[k] = {at(5 + k * 3, i), at(6 + k * 3, i),
                        at(7 + k * 3, i) / confidence_output_scale};
      if (p.keypoints[k].confidence > 0.25F)
        ++visible;
    }
    if (visible >= 4 && w >= 10.0F && h >= 15.0F)
      poses.push_back(p);
  }
  std::sort(poses.begin(), poses.end(), [](const Pose &a, const Pose &b) {
    return a.confidence > b.confidence;
  });
  std::vector<Pose> kept;
  for (const Pose &pose : poses) {
    bool suppressed = false;
    for (const Pose &accepted : kept)
      if (pose_iou(pose, accepted) > nms) {
        suppressed = true;
        break;
      }
    if (!suppressed)
      kept.push_back(pose);
  }
  return kept;
}
} // namespace

RknnPoseModel::RknnPoseModel(const std::string &path)
    : model_data_(read_file(path)) {
  require_rknn(
      rknn_init(&context_, model_data_.data(), model_data_.size(), 0, nullptr),
      "rknn_init");
  rknn_sdk_version version{};
  require_rknn(
      rknn_query(context_, RKNN_QUERY_SDK_VERSION, &version, sizeof(version)),
      "query SDK version");
  std::cout << "RKNN API " << version.api_version << ", driver "
            << version.drv_version << '\n';
  rknn_input_output_num io{};
  require_rknn(rknn_query(context_, RKNN_QUERY_IN_OUT_NUM, &io, sizeof(io)),
               "query I/O");
  if (io.n_input != 1 || io.n_output != 1)
    throw std::runtime_error("expected one input and one output");
  rknn_tensor_attr attr{};
  attr.index = 0;
  require_rknn(
      rknn_query(context_, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr)),
      "query output");
  if (attr.n_elems != kChannels * kCandidates)
    throw std::runtime_error("unexpected output element count: " +
                             std::to_string(attr.n_elems));
  // The INT8 conversion scales sigmoid confidence channels before the final
  // concat so coordinates and probabilities share a useful output range.
  confidence_output_scale_ =
      attr.type == RKNN_TENSOR_INT8 ? 640.0F : 1.0F;
}
RknnPoseModel::~RknnPoseModel() {
  if (context_)
    rknn_destroy(context_);
}
std::vector<Pose> RknnPoseModel::infer(const uint8_t *rgb, size_t size,
                                       float confidence, float nms) {
  rknn_input input{};
  input.index = 0;
  input.buf = const_cast<uint8_t *>(rgb);
  input.size = size;
  input.type = RKNN_TENSOR_UINT8;
  input.fmt = RKNN_TENSOR_NHWC;
  require_rknn(rknn_inputs_set(context_, 1, &input), "set input");
  require_rknn(rknn_run(context_, nullptr), "run");
  rknn_output output{};
  output.index = 0;
  output.want_float = 1;
  require_rknn(rknn_outputs_get(context_, 1, &output, nullptr), "get output");
  auto poses = decode(static_cast<const float *>(output.buf), confidence, nms,
                      confidence_output_scale_);
  require_rknn(rknn_outputs_release(context_, 1, &output), "release output");

  return poses;
}
