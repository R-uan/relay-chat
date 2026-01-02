#pragma once

#include "channel.hh"
#include "configurations.hh"
#include "typedef.hh"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

class ChannelManager {
public:
  bool has_capacity();
  void remove_channel(uint32_t i);
  Channel *find_channel(uint32_t i) const;
  std::vector<char> create_channel(uint32_t i, w_client c);

  ChannelManager(const ChannelManager &) = delete;
  ChannelManager &operator=(ChannelManager &) = delete;

  static ChannelManager &instance() {
    static ChannelManager manager(
        ServerConfiguration::instance().max_channels());
    return manager;
  }

private:
  ChannelManager(int max) : MAXCHANNELS(max) {};

private:
  std::shared_mutex mutex;
  const size_t MAXCHANNELS;
  std::unordered_map<uint32_t, std::unique_ptr<Channel>> channels;
};

class ClientManager {
public:
  bool has_capacity();

  int add_client(int fd);
  int add_client(ws_handle hdl);

  void remove_client(uint32_t fd);
  void remove_client(ws_handle &hdl);

  std::optional<std::shared_ptr<Client>> find_client(uint32_t fd) const;
  std::optional<std::shared_ptr<Client>> find_client(ws_handle &hdl) const;

  ClientManager(const ClientManager &) = delete;
  ClientManager &operator=(const ClientManager &) = delete;

  static ClientManager &instance() {
    static ClientManager manager(ServerConfiguration::instance().max_clients());
    return manager;
  }

private:
  ClientManager(int max) : MAXCLIENTS(max) {};

private:
  const size_t MAXCLIENTS;
  std::shared_mutex mutex;
  std::atomic_int clientIds{1};
  std::unordered_map<uint32_t, std::shared_ptr<Client>> tcp_clients_{};
  std::map<ws_handle, std::shared_ptr<Client>, std::owner_less<ws_handle>>
      ws_clients_{};
};
