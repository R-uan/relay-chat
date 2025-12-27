#pragma once
#include <mutex>

constexpr int MIN_CHANNELS = 1;
constexpr int MIN_CLIENTS = 10;
constexpr int MIN_THREADS = 5;

/*
 * Returns the lowest value of two.
 */
template <typename T> constexpr bool is_bigger(const T &input, const T &min) {
  return input > min;
}

class ServerConfiguration {
private:
  ServerConfiguration() = default;

  int port_ = 3000;
  bool debug_mode_ = false;
  int max_clients_ = MIN_CLIENTS;
  int max_channels_ = MIN_CHANNELS;
  int thread_pool_size_ = MIN_THREADS;

  // mutable
  int active_users_ = 0;
  mutable std::mutex mutex_;

public:
  ServerConfiguration(const ServerConfiguration &) = delete;
  ServerConfiguration &operator=(const ServerConfiguration &) = delete;

  static ServerConfiguration &instance() {
    static ServerConfiguration config;
    return config;
  }

  void set_port(int port) { port_ = port; }

  void set_max_channels(int size) {
    if (is_bigger(size, MIN_CHANNELS)) {
      std::unique_lock<std::mutex> lock(mutex_);
      max_channels_ = size;
    }
  }

  void set_max_clients(int size) {
    if (is_bigger(size, MIN_CLIENTS)) {
      std::unique_lock<std::mutex> lock(mutex_);
      max_clients_ = size;
    }
  }

  void set_pool_size(int size) {
    if (is_bigger(size, MIN_THREADS)) {
      std::unique_lock<std::mutex> lock(mutex_);
      thread_pool_size_ = size;
    }
  }

  bool debug_mode() const { return debug_mode_; }
  int port() const { return port_; }
  int max_clients() const { return max_clients_; }
  int active_users() const { return active_users_; }
  int max_channels() const { return max_channels_; }
  int pool_size() const { return thread_pool_size_; }
};
