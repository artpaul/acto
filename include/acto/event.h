#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace acto::core {

enum class wait_result {
  error,
  signaled,
  timeout,
};

class event {
public:
  explicit event(const bool auto_reset = false);

public:
  void reset();

  void signaled();

  wait_result wait();

  wait_result wait(const std::chrono::nanoseconds duration);

private:
  std::mutex mutex_;
  std::condition_variable cond_;
  bool auto_;
  bool triggered_;
};

} // namespace acto::core
