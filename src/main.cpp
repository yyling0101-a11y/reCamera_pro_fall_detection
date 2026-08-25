#include "fall_detector.hpp"
#include "fall_alarm.hpp"
#include "frame_overlay.hpp"
#include "rknn_pose_model.hpp"
#include "rtsp_streamer.hpp"
#include "websocket_server.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
std::atomic<bool> running{true};
struct Options {
  std::string model, device = "/dev/video13", websocket_bind = "0.0.0.0",
              alarm_sound = "/userdata/fall_detection/warning.wav";
  int websocket_port = 9000, rtsp_port = 8554, rtsp_fps = 10, max_frames = 0;
  int alarm_window = 15, alarm_votes = 8;
  float confidence = 0.35F, nms = 0.45F, inactivity_seconds = 1.5F;
  float alarm_cooldown = 10.0F;
  bool websocket_test = false, alarm_test = false, rtsp_enabled = true;
};
double monotonic_seconds() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}
Options parse_options(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    std::string argument = argv[i];
    auto value = [&]() {
      if (++i >= argc)
        throw std::runtime_error("missing value for " + argument);
      return std::string(argv[i]);
    };
    if (argument == "--model")
      options.model = value();
    else if (argument == "--device")
      options.device = value();
    else if (argument == "--ws-bind")
      options.websocket_bind = value();
    else if (argument == "--ws-port")
      options.websocket_port = std::stoi(value());
    else if (argument == "--rtsp-port")
      options.rtsp_port = std::stoi(value());
    else if (argument == "--rtsp-fps")
      options.rtsp_fps = std::stoi(value());
    else if (argument == "--no-rtsp")
      options.rtsp_enabled = false;
    else if (argument == "--confidence")
      options.confidence = std::stof(value());
    else if (argument == "--nms")
      options.nms = std::stof(value());
    else if (argument == "--inactivity-sec")
      options.inactivity_seconds = std::stof(value());
    else if (argument == "--alarm-sound")
      options.alarm_sound = value();
    else if (argument == "--alarm-window")
      options.alarm_window = std::stoi(value());
    else if (argument == "--alarm-votes")
      options.alarm_votes = std::stoi(value());
    else if (argument == "--alarm-cooldown")
      options.alarm_cooldown = std::stof(value());
    else if (argument == "--max-frames")
      options.max_frames = std::stoi(value());
    else if (argument == "--websocket-test")
      options.websocket_test = true;
    else if (argument == "--alarm-test")
      options.alarm_test = true;
    else if (argument == "--help") {
      std::cout << "Usage: " << argv[0]
                << " --model MODEL.rknn [--device /dev/video13] [--ws-port "
                   "9000] [--rtsp-port 8554] [--rtsp-fps 10] [--no-rtsp]\n";
      std::exit(0);
    } else
      throw std::runtime_error("unknown argument: " + argument);
  }
  if (options.model.empty() && !options.websocket_test && !options.alarm_test)
    throw std::runtime_error("--model is required");
  if (options.rtsp_fps <= 0)
    throw std::runtime_error("--rtsp-fps must be positive");
  if (options.alarm_window <= 0 || options.alarm_votes <= 0 ||
      options.alarm_votes > options.alarm_window ||
      options.alarm_cooldown < 0)
    throw std::runtime_error("invalid alarm queue configuration");
  return options;
}
} // namespace

int main(int argc, char **argv) {
  GstElement *pipeline = nullptr;
  GstElement *sink = nullptr;
  try {
    const Options options = parse_options(argc, argv);
    std::signal(SIGINT, [](int) { running = false; });
    std::signal(SIGTERM, [](int) { running = false; });
    gst_init(&argc, &argv);
    if (options.alarm_test) {
      FallAlarm alarm(options.alarm_sound,
                      static_cast<size_t>(options.alarm_window),
                      static_cast<size_t>(options.alarm_votes),
                      options.alarm_cooldown);
      const double now = monotonic_seconds();
      bool triggered = false;
      for (int i = 0; i < options.alarm_window; ++i)
        triggered = alarm.update(i < options.alarm_votes, now) || triggered;
      usleep(2000000);
      std::cout << "alarm_test_triggered=" << triggered << '\n';
      return triggered ? 0 : 1;
    }
    WebSocketServer websocket(options.websocket_bind, options.websocket_port);
    if (options.websocket_test) {
      const double deadline = monotonic_seconds() + 15.0;
      while (running && websocket.client_count() == 0 &&
             monotonic_seconds() < deadline) {
        websocket.service();
        usleep(20000);
      }
      const size_t sent = websocket.broadcast_text(
          "{\"type\":\"system_test\",\"source\":\"recamera_fall_detection\"}");
      std::cout << "websocket_test_clients_sent=" << sent << '\n';
      return sent > 0 ? 0 : 1;
    }
    RknnPoseModel model(options.model);
    FallDetector detector(options.inactivity_seconds, websocket);
    FallAlarm alarm(options.alarm_sound,
                    static_cast<size_t>(options.alarm_window),
                    static_cast<size_t>(options.alarm_votes),
                    options.alarm_cooldown);
    std::unique_ptr<RtspStreamer> rtsp;
    if (options.rtsp_enabled)
      rtsp =
          std::make_unique<RtspStreamer>(options.rtsp_port, options.rtsp_fps);
    std::ostringstream description;
    description
        << "v4l2src device=" << options.device
        << " ! video/x-raw,format=NV12,width=640,height=640,framerate=30/1 ! "
           "videoconvert ! "
           "video/x-raw,format=RGB,width=640,height=640,pixel-aspect-ratio=1/1 "
           "! appsink name=inference_sink max-buffers=1 drop=true sync=false";
    GError *error = nullptr;
    pipeline = gst_parse_launch(description.str().c_str(), &error);
    if (!pipeline) {
      const std::string message = error ? error->message : "unknown";
      if (error)
        g_error_free(error);
      throw std::runtime_error("GStreamer pipeline: " + message);
    }
    sink = gst_bin_get_by_name(GST_BIN(pipeline), "inference_sink");
    if (!sink)
      throw std::runtime_error("appsink missing");
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE)
      throw std::runtime_error("cannot start camera pipeline");
    int frames = 0;
    const double started = monotonic_seconds();
    double previous_frame = started, last_rtsp_push = 0;
    float current_fps = 0;
    while (running &&
           (options.max_frames <= 0 || frames < options.max_frames)) {
      websocket.service();
      GstSample *sample =
          gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 2 * GST_SECOND);
      if (!sample) {
        std::cerr << "camera timeout\n";
        continue;
      }
      GstBuffer *buffer = gst_sample_get_buffer(sample);
      GstMapInfo map{};
      if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        continue;
      }
      if (map.size != kInputSize * kInputSize * 3) {
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
        throw std::runtime_error("unexpected RGB buffer size");
      }
      auto poses =
          model.infer(map.data, map.size, options.confidence, options.nms);
      const double now = monotonic_seconds();
      const float instant = 1.0F / std::max(0.001, now - previous_frame);
      previous_frame = now;
      current_fps =
          current_fps == 0 ? instant : current_fps * 0.9F + instant * 0.1F;
      auto overlays = detector.update(poses, now);
      const bool frame_has_fall =
          std::any_of(overlays.begin(), overlays.end(), [](const auto &overlay) {
            return overlay.state == "CONFIRMED_FALL";
          });
      alarm.update(frame_has_fall, now);
      if (rtsp && now - last_rtsp_push >= 1.0 / options.rtsp_fps) {
        std::vector<uint8_t> annotated(map.data, map.data + map.size);
        annotate_frame(annotated, poses, overlays, current_fps);
        rtsp->push(annotated.data(), annotated.size());
        last_rtsp_push = now;
      }
      gst_buffer_unmap(buffer, &map);
      gst_sample_unref(sample);
      ++frames;
      if (frames % 30 == 0)
        std::cout << "frames=" << frames
                  << " average_fps=" << frames / (monotonic_seconds() - started)
                  << '\n';
    }
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(pipeline);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    if (pipeline)
      gst_element_set_state(pipeline, GST_STATE_NULL);
    if (sink)
      gst_object_unref(sink);
    if (pipeline)
      gst_object_unref(pipeline);
    return 1;
  }
}
