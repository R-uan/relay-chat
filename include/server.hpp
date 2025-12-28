#pragma once

#include "channel.hpp"
#include "client.hpp"
#include "configurations.hh"
#include "managers.hpp"
#include "spdlog/spdlog.h"
#include "thread_pool.hpp"
#include <arpa/inet.h>
#include <cstdlib>
#include <memory>
#include <netinet/in.h>
#include <shared_mutex>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

class Server : public std::enable_shared_from_this<Server> {
private:
  int epoll_fd_;
  int server_fd_;
  std::shared_mutex epoll_mtx_;

  int read_size(w_client pointer); // *
  int read_incoming(std::shared_ptr<Client> client);

  // Server Related Request Handlers
  // SVR_CONNECT handler is builtin the read_incoming
  // SRV_MESSAGE is exclusive to server -> client so it doesn't have a handler.
  void srv_disconnect(const w_client &client);

  // Channel Related Request Handlers
  Response ch_connect(w_client &client, const Request &request);
  Response ch_command(const w_client &client, const Request &request);
  Response ch_message(const w_client &client, const Request &request);
  Response ch_disconnect(const w_client &client, const Request &request);

public:
  std::unique_ptr<ClientManager> clients;
  std::unique_ptr<ChannelManager> channels;

  Server() {
    auto &config = ServerConfiguration::instance();

    // global thread pool first access
    ThreadPool::initialize();
    this->server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    this->clients = std::make_unique<ClientManager>(config.max_clients());
    this->channels = std::make_unique<ChannelManager>(config.max_channels());

    if (this->server_fd_ == -1) {
      spdlog::error("could not create server socket.");
      exit(1);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port());
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(this->server_fd_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      spdlog::error("unable to bind server to given address: {0}",
                    config.port());
      close(this->server_fd_);
      exit(2);
    }

    if (::listen(this->server_fd_, SOMAXCONN) == -1) {
      spdlog::error("socket failed to listen on bound address");
      close(this->server_fd_);
      exit(3);
    }

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = this->server_fd_;
    this->epoll_fd_ = epoll_create1(0);
    epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, this->server_fd_, &ev);

    spdlog::info("server setup complete");
    spdlog::info("listening on port {0}", config.port());
    spdlog::info("thread pool size {0}", config.pool_size());
    spdlog::info("max clients allowed {0}", config.max_clients());
    spdlog::info("max channels allowed {0}", config.max_channels());
  }

  ~Server() {
    close(this->epoll_fd_);
    close(this->server_fd_);
  }

  void listen();
  void destroy_channel(int id);
};
