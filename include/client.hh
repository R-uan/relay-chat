#pragma once

#include "configurations.hh"
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
  bool admin{false};
  std::string username;
  ClientTransport transport;
  std::optional<ws_handle> ws_hld;
  std::vector<uint32_t> channels{};
  std::atomic_bool connected{false};

public:
  bool is_member(const int channel_id);
  bool send_packet(const Response packet);

  void set_connection(bool b);
  void add_channel(const int channel_id);
  void remove_channel(const int channel_id);

  void set_admin(const std::vector<uint8_t> password);
  std::string change_username(const std::vector<uint8_t> username);

  explicit Client(int fd, int id)
      : fd(fd), id(id), username(std::format("user0{}", id)),
        transport(ClientTransport::TCP), ws_hld(std::nullopt) {}

  explicit Client(int id, ws_handle hdl)
      : fd(-1), id(id), username(std::format("user0{}", id)),
        transport(ClientTransport::WBS), ws_hld(hdl) {}

  ~Client() {
    if (this->fd != -1) {
      close(this->fd);
    }
    spdlog::debug("client destroyed {0}", this->username);
  }
};
