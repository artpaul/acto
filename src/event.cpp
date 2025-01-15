#include "acto/event.h"

namespace acto::core {

event::event(const bool auto_reset)
  : auto_(auto_reset)
  , triggered_(false) {
}

void event::reset() {
  std::lock_guard g(mutex_);

  triggered_ = false;
}

void event::signaled() {
  std::lock_guard g(mutex_);

  triggered_ = true;
  cond_.notify_one();
}

wait_result event::wait() {
  std::unique_lock g(mutex_);

  cond_.wait(g, [this] { return triggered_; });

  triggered_ = !auto_;

  return wait_result::signaled;
}

wait_result event::wait(const std::chrono::nanoseconds duration) {
  const auto deadline = std::chrono::high_resolution_clock::now() + duration;
  std::unique_lock g(mutex_);

  if (!cond_.wait_until(g, deadline, [this] { return triggered_; })) {
    return wait_result::timeout;
  }

  triggered_ = !auto_;

  return wait_result::signaled;
}

} // namespace acto::core
