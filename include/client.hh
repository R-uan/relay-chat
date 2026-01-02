#pragma once

#include "spdlog/spdlog.h"
#include "typedef.hh"
#include "utilities.hh"
#include <atomic>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

enum class ClientTransport { TCP, WBS };
// Shared Pointer Tracker (Where a client shared_ptr can be found)
// # Server
//   -> client unordered map
// # Channel
//   -> chatter vector
//   -> moderator vector
//   -> emperor
class Client {
public:
  int fd;
  int id;
  std::mutex mtx;
  std::string username;
  std::optional<ws_handle> ws_hld;
  std::vector<uint32_t> channels{};
  std::atomic_bool connected{false};
  ClientTransport transport;

  void set_connection(bool b);
  bool is_member(const int channel_id);
  void add_channel(const int channel_id);
  void remove_channel(const int channel_id);

  virtual bool send_packet(const Response packet);
  std::string change_username(std::string username);

  explicit Client(int fd, int id) : transport(ClientTransport::TCP) {
    std::string username = std::format("user0{0}", id);
    this->ws_hld = std::nullopt;
    this->username = username;
    this->fd = fd;
    this->id = id;
  }

  explicit Client(int id, ws_handle hdl) : transport(ClientTransport::WBS) {
    std::string username = std::format("user0{0}", id);
    this->username = username;
    this->ws_hld = hdl;
    this->fd = -1;
    this->id = id;
  }

  virtual ~Client() {
    if (this->fd != -1) {
      close(this->fd);
    }
    spdlog::debug("client destroyed {0}", this->username);
  }
};
