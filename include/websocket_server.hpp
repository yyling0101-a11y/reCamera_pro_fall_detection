#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class WebSocketServer {
public:
  WebSocketServer(std::string bind_address, int port);
  ~WebSocketServer();
  void service();
  size_t client_count() const;
  size_t broadcast_text(const std::string &text);

private:
  struct Peer {
    int fd;
    std::vector<uint8_t> rx;
  };
  void start();
  void accept_clients();
  bool service_peer(Peer &peer);
  static std::vector<uint8_t> make_frame(uint8_t opcode, const uint8_t *data,
                                         size_t size);
  static bool write_all(int fd, const uint8_t *data, size_t size);
  std::string bind_address_;
  int port_;
  int listener_ = -1;
  std::vector<Peer> peers_;
};
