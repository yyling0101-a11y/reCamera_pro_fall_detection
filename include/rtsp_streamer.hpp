#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

class RtspStreamer {
public:
  RtspStreamer(int port, int fps);
  ~RtspStreamer();
  void push(const uint8_t *rgb, size_t size);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
