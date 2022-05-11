#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>

namespace acto {
namespace core {

enum class wait_result {
  error,
  signaled,
  timeout,
};

/** Событие */
class event_t {
public:
  event_t(const bool auto_reset = false);
  ~event_t();

public:
  ///
  void reset();
  ///
  void signaled();
  ///
  wait_result wait();
  wait_result wait(const std::chrono::milliseconds msec);

private:
  const bool m_auto;
  bool m_triggered;

  std::mutex m_mutex;
  std::condition_variable m_cond;
};

} // namespace core
} // namespace acto
