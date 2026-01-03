#pragma once
#include <mutex>
#include <string>

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
  std::string secret_password = "password";
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

  inline void set_port(int port) { port_ = port; }

  inline void set_debug() { debug_mode_ = true; }

  inline void set_max_channels(int size) {
    if (is_bigger(size, MIN_CHANNELS)) {
      std::unique_lock<std::mutex> lock(mutex_);
      max_channels_ = size;
    }
  }

  inline void set_max_clients(int size) {
    if (is_bigger(size, MIN_CLIENTS)) {
      std::unique_lock<std::mutex> lock(mutex_);
      max_clients_ = size;
    }
  }

  inline void set_pool_size(int size) {
    if (is_bigger(size, MIN_THREADS)) {
      std::unique_lock<std::mutex> lock(mutex_);
      thread_pool_size_ = size;
    }
  }

  inline void set_password(std::string secret) {
    this->secret_password = secret;
  }

  inline std::string secret() { return secret_password; }
  inline bool debugging() const { return debug_mode_; }
  inline int port() const { return port_; }
  inline int max_clients() const { return max_clients_; }
  inline int active_users() const { return active_users_; }
  inline int max_channels() const { return max_channels_; }
  inline int pool_size() const { return thread_pool_size_; }
};
