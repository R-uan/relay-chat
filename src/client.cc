#include "client.hh"
#include "configurations.hh"
#include <algorithm>
#include <cstddef>
#include <format>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/socket.h>
#include <vector>

void Client::add_channel(const int channelId) {
  std::unique_lock lock(this->mtx);
  this->channels.push_back(channelId);
}

void Client::remove_channel(const int channelId) {
  std::unique_lock lock(this->mtx);
  std::erase_if(this->channels,
                [&](const int &channel) { return channel == channelId; });
}

bool Client::send_packet(const Response packet) {
  auto data = packet.data;
  if (send(this->fd, data.data(), data.size(), 0) == -1) {
    return false;
  }
  return true;
}

bool Client::is_member(const int channelId) {
  return std::find_if(this->channels.begin(), this->channels.end(),
                      [&](const int id) { return id == channelId; }) !=
         this->channels.end();
}

void Client::set_connection(bool b) {
  this->connected.exchange(b);
  spdlog::debug("{} connection status changed: {}", this->username, b);
}

std::string Client::change_username(const std::vector<uint8_t> bytes) {
  std::unique_lock lock(this->mtx);
  std::string username(bytes.begin(), bytes.end());
  this->username = std::format("{0}{1}", username, this->id);
  return this->username;
}

void Client::set_admin(const std::vector<uint8_t> bytes) {
  std::string password(bytes.begin(), bytes.end());
  if (password == ServerConfiguration::instance().secret()) {
    spdlog::debug("{} registered as an admin", this->username);
    std::unique_lock lock(this->mtx);
    this->admin = true;
  }
}
