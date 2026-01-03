#include "managers.hh"
#include "channel.hh"
#include "client.hh"
#include "typedef.hh"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <sys/types.h>
#include <utility>
#include <vector>

bool ChannelManager::has_capacity() {
  return this->MAXCHANNELS > this->channels.size();
}

void ChannelManager::remove_channel(uint32_t i) {
  std::unique_lock lock(this->mutex);
  this->channels.erase(i);
}

Channel *ChannelManager::find_channel(uint32_t i) const {
  auto find = this->channels.find(i);
  if (find == this->channels.end()) {
    return nullptr;
  }
  return find->second.get();
}

bool ClientManager::has_capacity() {
  return this->MAXCLIENTS > this->tcp_clients_.size();
}

int ClientManager::add_client(int fd) {
  int clientId = this->clientIds;
  auto sclient = std::make_shared<Client>(fd, clientId);
  this->clientIds.fetch_add(1);
  std::unique_lock lock(this->mutex);
  this->tcp_clients_.emplace(fd, std::move(sclient));
  return clientId;
}

int ClientManager::add_client(ws_handle hdl) {
  int clientId = this->clientIds;
  auto sclient = std::make_shared<Client>(clientId, hdl);
  this->clientIds.fetch_add(1);
  std::unique_lock lock(this->mutex);
  this->ws_clients_.emplace(hdl, std::move(sclient));
  return clientId;
}

void ClientManager::remove_client(ws_handle &hdl) {
  std::unique_lock lock(this->mutex);
  this->ws_clients_.erase(hdl);
}

void ClientManager::remove_client(uint32_t fd) {
  std::unique_lock lock(this->mutex);
  this->tcp_clients_.erase(fd);
}

std::optional<std::shared_ptr<Client>>
ClientManager::find_client(uint32_t fd) const {
  auto find = this->tcp_clients_.find(fd);
  if (find == this->tcp_clients_.end()) {
    return std::nullopt;
  }
  return find->second;
}

std::optional<std::shared_ptr<Client>>
ClientManager::find_client(ws_handle &hdl) const {
  auto find = this->ws_clients_.find(hdl);
  if (find == this->ws_clients_.end()) {
    return std::nullopt;
  }
  return find->second;
}

std::vector<ChannelView> ChannelManager::get_views() {
  std::vector<ChannelView> views;
  for (const auto &[key, value] : this->channels) {
    auto view = value->get_view();
    views.push_back(view);
  }
  return views;
}

std::vector<char> ChannelManager::create_channel(std::string name,
                                                 bool secret) {
  auto channel = std::make_unique<Channel>(this->channel_id_tracker_, name);
  spdlog::debug("New channel created: {}:{}", channel->id, channel->name);
  this->channel_id_tracker_.fetch_add(1);
  channel->secret.exchange(secret);
  auto info = channel->info();

  this->channels.emplace(channel->id, std::move(channel));
  return info;
}
