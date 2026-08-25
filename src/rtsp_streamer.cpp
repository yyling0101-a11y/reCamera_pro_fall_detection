#include "rtsp_streamer.hpp"
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

struct RtspStreamer::Impl {
  explicit Impl(int port, int output_fps) : fps(output_fps) {
    server = gst_rtsp_server_new();
    const std::string service = std::to_string(port);
    gst_rtsp_server_set_service(server, service.c_str());
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
    const std::string launch =
        "( appsrc name=fall_source is-live=true block=false format=time "
        "do-timestamp=true "
        "caps=video/x-raw,format=RGB,width=640,height=640,framerate=" +
        std::to_string(fps) +
        "/1 ! queue leaky=downstream max-size-buffers=2 ! videoconvert ! "
        "video/x-raw,format=I420 ! jpegenc quality=78 ! rtpjpegpay name=pay0 "
        "pt=96 )";
    gst_rtsp_media_factory_set_launch(factory, launch.c_str());
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    g_signal_connect(factory, "media-configure", G_CALLBACK(configure), this);
    gst_rtsp_mount_points_add_factory(mounts, "/fall", factory);
    g_object_unref(mounts);
    if (gst_rtsp_server_attach(server, nullptr) == 0)
      throw std::runtime_error("cannot attach RTSP server on port " + service);
    loop = g_main_loop_new(nullptr, FALSE);
    thread = std::thread([this] { g_main_loop_run(loop); });
    std::cout << "RTSP stream ready at rtsp://0.0.0.0:" << port
              << "/fall (RTP/JPEG)\n";
  }
  ~Impl() {
    if (loop)
      g_main_loop_quit(loop);
    if (thread.joinable())
      thread.join();
    std::lock_guard<std::mutex> lock(mutex);
    if (source)
      gst_object_unref(source);
    if (loop)
      g_main_loop_unref(loop);
    if (server)
      g_object_unref(server);
  }
  void push(const uint8_t *rgb, size_t size) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!source)
      return;
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    gst_buffer_fill(buffer, 0, rgb, size);
    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, fps);
    const GstFlowReturn result =
        gst_app_src_push_buffer(GST_APP_SRC(source), buffer);
    if (result != GST_FLOW_OK && result != GST_FLOW_FLUSHING)
      std::cerr << "RTSP appsrc flow=" << gst_flow_get_name(result) << '\n';
  }
  static void configure(GstRTSPMediaFactory *, GstRTSPMedia *media,
                        gpointer data) {
    auto *self = static_cast<Impl *>(data);
    GstElement *element = gst_rtsp_media_get_element(media);
    GstElement *new_source =
        gst_bin_get_by_name_recurse_up(GST_BIN(element), "fall_source");
    gst_object_unref(element);
    if (!new_source)
      return;
    std::lock_guard<std::mutex> lock(self->mutex);
    if (self->source)
      gst_object_unref(self->source);
    self->source = new_source;
  }
  int fps;
  GstRTSPServer *server = nullptr;
  GMainLoop *loop = nullptr;
  std::thread thread;
  std::mutex mutex;
  GstElement *source = nullptr;
};
RtspStreamer::RtspStreamer(int port, int fps)
    : impl_(std::make_unique<Impl>(port, fps)) {}
RtspStreamer::~RtspStreamer() = default;
void RtspStreamer::push(const uint8_t *rgb, size_t size) {
  impl_->push(rgb, size);
}
