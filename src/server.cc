#include "server.hh"
#include "client.hh"
#include "protocol.hh"
#include "spdlog/spdlog.h"
#include "thread_pool.hh"
#include "typedef.hh"
#include "utilities.hh"
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
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
            auto res =
                response(-1, SVR_CONNECT, (std::string) "server is full");
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
              this->disconnect(client);
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
  int packetSize = read_size(s_client);
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
  Request request(buffer);
  Response response = Protocol::handle_request(s_client, request);

  if (response.size > 0) {
    s_client->send_packet(response);
  }

  return 0;
}

/* Reads the first 4 bytes of a fd buffer to get the request's size.
 * Returns -1 if the client weak pointer fails to lock
 */
int Server::read_size(w_client w_client) {
  auto client = w_client.lock();

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
void Server::disconnect(const w_client &w_client) {}
