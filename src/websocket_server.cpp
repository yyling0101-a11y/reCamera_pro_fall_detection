#include "websocket_server.hpp"
#include <arpa/inet.h>
#include <array>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace {
std::string base64(const uint8_t *data, size_t size) {
  static constexpr char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < size; i += 3) {
    uint32_t value = static_cast<uint32_t>(data[i]) << 16;
    if (i + 1 < size)
      value |= static_cast<uint32_t>(data[i + 1]) << 8;
    if (i + 2 < size)
      value |= data[i + 2];
    out.push_back(table[(value >> 18) & 63]);
    out.push_back(table[(value >> 12) & 63]);
    out.push_back(i + 1 < size ? table[(value >> 6) & 63] : '=');
    out.push_back(i + 2 < size ? table[value & 63] : '=');
  }
  return out;
}
std::array<uint8_t, 20> sha1(const std::string &input) {
  std::vector<uint8_t> msg(input.begin(), input.end());
  const uint64_t bits = static_cast<uint64_t>(msg.size()) * 8;
  msg.push_back(0x80);
  while (msg.size() % 64 != 56)
    msg.push_back(0);
  for (int i = 7; i >= 0; --i)
    msg.push_back(static_cast<uint8_t>(bits >> (i * 8)));
  uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476,
           h4 = 0xC3D2E1F0;
  for (size_t off = 0; off < msg.size(); off += 64) {
    uint32_t w[80]{};
    for (int i = 0; i < 16; ++i)
      w[i] = (msg[off + i * 4] << 24) | (msg[off + i * 4 + 1] << 16) |
             (msg[off + i * 4 + 2] << 8) | msg[off + i * 4 + 3];
    for (int i = 16; i < 80; ++i) {
      uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      w[i] = (v << 1) | (v >> 31);
    }
    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = t;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }
  std::array<uint8_t, 20> out{};
  uint32_t hs[5] = {h0, h1, h2, h3, h4};
  for (int i = 0; i < 5; ++i)
    for (int j = 0; j < 4; ++j)
      out[i * 4 + j] = static_cast<uint8_t>(hs[i] >> (24 - j * 8));
  return out;
}
} // namespace

WebSocketServer::WebSocketServer(std::string bind_address, int port)
    : bind_address_(std::move(bind_address)), port_(port) {
  start();
}
WebSocketServer::~WebSocketServer() {
  for (const Peer &peer : peers_)
    ::close(peer.fd);
  if (listener_ >= 0)
    ::close(listener_);
}
void WebSocketServer::service() {
  accept_clients();
  for (size_t i = 0; i < peers_.size();) {
    if (service_peer(peers_[i]))
      ++i;
    else {
      ::close(peers_[i].fd);
      peers_.erase(peers_.begin() + i);
    }
  }
}
size_t WebSocketServer::client_count() const { return peers_.size(); }
size_t WebSocketServer::broadcast_text(const std::string &text) {
  const auto frame = make_frame(
      0x1, reinterpret_cast<const uint8_t *>(text.data()), text.size());
  size_t sent = 0;
  for (size_t i = 0; i < peers_.size();) {
    if (write_all(peers_[i].fd, frame.data(), frame.size())) {
      ++sent;
      ++i;
    } else {
      ::close(peers_[i].fd);
      peers_.erase(peers_.begin() + i);
    }
  }
  return sent;
}
void WebSocketServer::start() {
  if (port_ <= 0 || port_ > 65535)
    throw std::runtime_error("invalid WebSocket port");
  listener_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listener_ < 0)
    throw std::runtime_error("cannot create WebSocket listener");
  int yes = 1;
  setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port_);
  if (bind_address_ == "0.0.0.0")
    address.sin_addr.s_addr = htonl(INADDR_ANY);
  else if (inet_pton(AF_INET, bind_address_.c_str(), &address.sin_addr) != 1)
    throw std::runtime_error("--ws-bind must be an IPv4 address");
  if (bind(listener_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
      0)
    throw std::runtime_error("cannot bind WebSocket server");
  if (listen(listener_, 8) < 0)
    throw std::runtime_error("cannot listen for WebSocket clients");
  fcntl(listener_, F_SETFL, fcntl(listener_, F_GETFL, 0) | O_NONBLOCK);
  std::cout << "WebSocket server listening on ws://" << bind_address_ << ':'
            << port_ << "/alerts\n";
}
void WebSocketServer::accept_clients() {
  while (true) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    int fd = accept(listener_, reinterpret_cast<sockaddr *>(&address), &length);
    if (fd < 0)
      break;
    timeval timeout{2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    std::string request;
    char buffer[1024];
    while (request.find("\r\n\r\n") == std::string::npos &&
           request.size() < 8192) {
      const ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
      if (n <= 0)
        break;
      request.append(buffer, n);
    }
    const std::string marker = "Sec-WebSocket-Key:";
    const size_t begin = request.find(marker);
    if (request.rfind("GET /alerts ", 0) != 0 || begin == std::string::npos) {
      ::close(fd);
      continue;
    }
    size_t value_begin = begin + marker.size();
    while (value_begin < request.size() && request[value_begin] == ' ')
      ++value_begin;
    const size_t value_end = request.find("\r\n", value_begin);
    const auto digest =
        sha1(request.substr(value_begin, value_end - value_begin) +
             "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    const std::string response =
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: "
        "Upgrade\r\nSec-WebSocket-Accept: " +
        base64(digest.data(), digest.size()) + "\r\n\r\n";
    if (!write_all(fd, reinterpret_cast<const uint8_t *>(response.data()),
                   response.size())) {
      ::close(fd);
      continue;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    peers_.push_back({fd, {}});
    std::cout << "WebSocket client connected: " << inet_ntoa(address.sin_addr)
              << '\n';
  }
}
bool WebSocketServer::service_peer(Peer &peer) {
  uint8_t chunk[1024];
  while (true) {
    const ssize_t n = recv(peer.fd, chunk, sizeof(chunk), MSG_DONTWAIT);
    if (n > 0)
      peer.rx.insert(peer.rx.end(), chunk, chunk + n);
    else if (n == 0)
      return false;
    else
      break;
  }
  while (peer.rx.size() >= 2) {
    const uint8_t opcode = peer.rx[0] & 0x0F;
    size_t header = 2;
    uint64_t length = peer.rx[1] & 0x7F;
    if (length == 126) {
      if (peer.rx.size() < 4)
        return true;
      length = (peer.rx[2] << 8) | peer.rx[3];
      header = 4;
    } else if (length == 127) {
      if (peer.rx.size() < 10)
        return true;
      length = 0;
      for (int i = 2; i < 10; ++i)
        length = (length << 8) | peer.rx[i];
      header = 10;
    }
    const bool masked = peer.rx[1] & 0x80;
    if (masked)
      header += 4;
    if (length > 65536)
      return false;
    if (peer.rx.size() < header + length)
      return true;
    std::vector<uint8_t> payload(peer.rx.begin() + header,
                                 peer.rx.begin() + header + length);
    if (masked) {
      const size_t mask_at = header - 4;
      for (size_t i = 0; i < payload.size(); ++i)
        payload[i] ^= peer.rx[mask_at + i % 4];
    }
    peer.rx.erase(peer.rx.begin(), peer.rx.begin() + header + length);
    if (opcode == 0x8)
      return false;
    if (opcode == 0x9) {
      const auto pong = make_frame(0xA, payload.data(), payload.size());
      if (!write_all(peer.fd, pong.data(), pong.size()))
        return false;
    }
  }
  return true;
}
std::vector<uint8_t>
WebSocketServer::make_frame(uint8_t opcode, const uint8_t *data, size_t size) {
  std::vector<uint8_t> frame{static_cast<uint8_t>(0x80 | opcode)};
  if (size < 126)
    frame.push_back(size);
  else if (size <= 65535) {
    frame.push_back(126);
    frame.push_back(size >> 8);
    frame.push_back(size);
  } else
    return {};
  frame.insert(frame.end(), data, data + size);
  return frame;
}
bool WebSocketServer::write_all(int fd, const uint8_t *data, size_t size) {
  while (size) {
    ssize_t n = send(fd, data, size, MSG_NOSIGNAL);
    if (n <= 0)
      return false;
    data += n;
    size -= n;
  }
  return true;
}
