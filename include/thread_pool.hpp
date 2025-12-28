#pragma once

#include "configurations.hh"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>
class ThreadPool {
private:
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic_bool stop{false};
  std::vector<std::thread> threads;
  std::queue<std::function<void()>> tasks;

  ThreadPool(int size) {
    for (int t = 0; t < size; t++) {
      this->threads.emplace_back([this]() {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock lock(this->mtx);
            this->cv.wait(lock, [this]() { return stop || !tasks.empty(); });

            if (this->stop && this->tasks.empty())
              return;

            task = std::move(this->tasks.front());
            this->tasks.pop();
          }

          task();
        }
      });
    }
  }

public:
  ThreadPool(const ServerConfiguration &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  ~ThreadPool() {
    {
      std::unique_lock lock(this->mtx);
      stop.exchange(true);
    }
    this->cv.notify_all();
  }

  template <typename F> inline void enqueue(F &&f) {
    {
      std::unique_lock lock(this->mtx);
      this->tasks.emplace(std::forward<F>(f));
    }
    this->cv.notify_one();
  }

  static ThreadPool &initialize() {
    static ThreadPool pool(ServerConfiguration::instance().pool_size());
    return pool;
  }
};
