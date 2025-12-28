#include "server.hpp"
#include "channel.hpp"
#include "client.hpp"
#include "managers.hpp"
#include "spdlog/spdlog.h"
#include "thread_pool.hpp"
#include "utilities.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

// * Utilises EPOLL to monitor new inputs on the server and client's file
// descriptors.
//
// * Handles new client connections and new incoming request from already
// stablished clients.
void Server::listen() {
  spdlog::info("server is now listening");
  epoll_event events[50];
  while (true) {
    int nfds = epoll_wait(this->epoll_fd_, events, 50, -1);
    for (int i = 0; i < nfds; i++) {
      int fd = events[i].data.fd;
      if (fd == this->server_fd_) {
        int ncfd = accept(this->server_fd_, nullptr, nullptr);
        if (ncfd != -1) {
          if (this->clients->has_capacity()) {
            epoll_event event;
            event.data.fd = ncfd;
            event.events = EPOLLIN | EPOLLONESHOT;
            {
              std::unique_lock lock(this->epoll_mtx_);
              epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, ncfd, &event);
            }
            this->clients->add_client(ncfd);
          } else {
            spdlog::warn("server capacity is full.");
            auto res = c_response(-3, DATAKIND::SVR_CONNECT, "server is full");
            auto payload = res.data;
            send(ncfd, payload.data(), payload.size(), 0);
            continue;
          }
        }
      } else {
        std::shared_lock lock(this->epoll_mtx_);
        auto find = this->clients->find_client(fd);
        if (find != std::nullopt) {
          std::shared_ptr<Client> client = find.value();
          ThreadPool::initialize().enqueue([this, client]() {
            int result = this->read_incoming(client);
            // * Result can be 0 or -1
            // *  0 : Rearms the client's event watcher
            // * -1 : Disconnects the client
            if (result == 0) {
              epoll_event event;
              event.data.fd = client->fd;
              event.events = EPOLLIN | EPOLLONESHOT;
              std::unique_lock lock(this->epoll_mtx_);
              epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client->fd, &event);
            } else {
              this->srv_disconnect(client);
              epoll_ctl(this->epoll_fd_, EPOLL_CTL_DEL, client->fd, nullptr);
            }
          });
        }
      }
    }
  }
}

/* Reads and redirects requests to appropriate handlers
 */
int Server::read_incoming(std::shared_ptr<Client> s_client) {
  int packetSize = this->read_size(s_client);
  if (packetSize <= 0) {
    return -1;
  }

  std::vector<uint8_t> buffer;
  buffer.resize(packetSize);
  {
    std::unique_lock lock(s_client->mtx);
    if (recv(s_client->fd, buffer.data(), packetSize, 0) <= 0) {
      return -1;
    }
  }
  Response response{};
  Request request(buffer);
  if (!s_client->connected) {
    if (request.type != DATAKIND::SVR_CONNECT) {
      response = c_response(-1, DATAKIND::SVR_CONNECT, "connection needed");
    } else {
      auto payload = request.payload;
      std::string username(payload.begin(), payload.end());
      // returns a formatted username: `username@id`
      std::string fmtusername = s_client->change_username(username);
      response = c_response(request.id, DATAKIND::SVR_CONNECT, fmtusername);
      spdlog::debug("new client: {0}", fmtusername);
      s_client->set_connection(true);
    }
  } else {
    std::weak_ptr<Client> wclient = s_client;
    switch (request.type) {
    case DATAKIND::CH_CONNECT:
      response = this->ch_connect(wclient, request);
      break;
    case DATAKIND::CH_DISCONNECT:
      response = this->ch_disconnect(wclient, request);
      break;
    case DATAKIND::SVR_DISCONNECT:
      return -1;
      break;
    case DATAKIND::CH_MESSAGE:
      response = this->ch_message(wclient, request);
      break;
    }
  }

  if (response.size > 0) {
    s_client->send_packet(response);
  }

  return 0;
}

/* Reads the first 4 bytes of a fd buffer to get the request's size.
 * Returns -1 if the client weak pointer fails to lock
 */
int Server::read_size(w_client pointer) {
  auto client = pointer.lock();

  if (!client)
    return -1;

  std::vector<uint8_t> buffer(4);
  {
    std::unique_lock lock(client->mtx);
    if (recv(client->fd, buffer.data(), 4, 0) <= 0) {
      return -1;
    }
  }
  return i32_from_le(buffer);
}

/* Removes the client accross the application by lowering the shared_ptr
 * counter to zero.
 *
 * Pointer tracker:
 *  - Server  -> clients::unordered_map
 *  - Channel -> chatters::vector
 *  - Channel -> moderators::vector
 *  - Channel -> emperor::shared_ptr
 */
void Server::srv_disconnect(const w_client &w_client) {
  auto s_client = w_client.lock();
  s_client->connected.exchange(false);
  // loop over the client's connected channels
  // find said channels
  // diconnect client from it
  for (int id : s_client->channels) {
    auto channel = this->channels->find_channel(id);
    if (channel != nullptr) {
      // if the disconnect_member returns true it means the channel must be
      // deleted
      if (channel->disconnect_member(s_client)) {
        spdlog::debug("{0} flagged for deletion", channel->name);
        this->channels->remove_channel(id);
      }
    }
  }

  this->clients->remove_client(s_client->fd);
  spdlog::info("{0} disconnected from the server", s_client->username);
}

// CHANNEL RELATED REQUEST HANDLERS

/* Request to join a channel.
 * The JOIN packet payload will be composed of: <flag> \n <channel> \n <token>
 * - <flag>    : channel creation flag
 * - <channel> : target channel's id (int) to join.
 * - <token>   : invitation token (optional).
 */
Response Server::ch_connect(w_client &w_client, const Request &request) {
  auto body = request.payload;
  if (body.size() < 5)
    return c_response(-1, DATAKIND::CH_CONNECT, "invalid packet");

  bool creation_flag = body[0] == 1; // create channel if doesn't exist ?
  int channel_id = i32_from_le({body[1], body[2], body[3], body[4]});
  auto channel = this->channels->find_channel(channel_id);

  if (channel == nullptr) { // channel doesn't exist...
    // ...and will be created
    if (creation_flag && this->channels->has_capacity()) {
      std::weak_ptr<Server> w_server = weak_from_this();
      auto info = channels->create_channel(channel_id, w_client, w_server);
      return c_response(request.id, DATAKIND::CH_CONNECT, info);
    }
    // ...and won't be created
    return c_response(-1, DATAKIND::CH_CONNECT);
  } else { // channel exists and the client...
    // ...successfully joined
    if (channel->enter_channel(w_client)) {
      auto s_client = w_client.lock();
      s_client->add_channel(channel_id);
      auto channel_info = channel->info();
      spdlog::debug("{0} joined {1}", s_client->username, channel->name);
      return c_response(request.id, DATAKIND::CH_CONNECT, channel_info);
    }
    // ...couldn't join
    return c_response(-1, DATAKIND::CH_CONNECT);
  }
}

/* Disconnects the client from the channel.
 * - If channel may be flagged for deletion.
 */
Response Server::ch_disconnect(const w_client &w_client,
                               const Request &request) {
  if (request.payload.size() >= 4) {
    auto payload = request.payload;
    auto channel_id = i32_from_le(payload);
    auto channel = this->channels->find_channel(channel_id);

    if (channel != nullptr) {
      auto s_client = w_client.lock();
      s_client->remove_channel(channel_id);
      spdlog::debug("{0} left {1}", s_client->username, channel->name);

      if (channel->disconnect_member(w_client)) {
        spdlog::debug("{0} flagged for deletion", channel->name);
        this->channels->remove_channel(channel_id);
      }

      return c_response(request.id, DATAKIND::CH_DISCONNECT);
    }
  }

  return c_response(-1, DATAKIND::CH_DISCONNECT);
}

/* Sends message in a channel.
 * - Checks if the channel exists
 * - Checks if the client is in the channel.
 */
Response Server::ch_message(const w_client &w_client, const Request &request) {
  const auto payload = request.payload;
  const uint32_t channel_id = i32_from_le(payload);
  const std::string message(payload.begin() + 4, payload.end());
  const auto channel = this->channels->find_channel(channel_id);

  if (channel != nullptr) {
    if (w_client.lock()->is_member(channel_id)) {
      channel->send_message(w_client, message);
      return c_response(request.id, DATAKIND::CH_MESSAGE);
    }
  }

  return c_response(-1, DATAKIND::CH_MESSAGE);
}

/* Maps command request to their respective handlers
 */
Response Server::ch_command(const w_client &w_client, const Request &request) {
  const auto payload = request.payload;
  const auto command_id = payload[0];
  const auto channel_id = i32_from_le(payload);
  const std::string message(payload.begin() + 5, payload.end());
  const auto channel = this->channels->find_channel(channel_id);

  if (channel != nullptr) {
    if (w_client.lock()->is_member(channel_id)) {
      switch (command_id) {
      case COMMAND::RENAME:
        channel->set_channel_name(w_client, message);
        break;
      case COMMAND::PIN:
        channel->pin_message(w_client, message);
        break;
      }
      return c_response(request.id, DATAKIND::CH_MESSAGE, "");
    }
  }

  return c_response(-1, DATAKIND::CH_MESSAGE);
}
