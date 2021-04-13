#include "event.h"

namespace acto {
namespace core {

using unique_lock = std::unique_lock<std::mutex>;

event_t::event_t(const bool auto_reset)
  : m_auto(auto_reset)
  , m_triggered(false)
{
}

event_t::~event_t() = default;

void event_t::reset() {
  unique_lock g(m_mutex);

  m_triggered = false;
}

void event_t::signaled() {
  unique_lock g(m_mutex);

  m_triggered = true;
  m_cond.notify_one();
}

wait_result event_t::wait() {
  unique_lock g(m_mutex);

  while (!m_triggered) {
    m_cond.wait(g);
  }

  if (m_auto) {
    m_triggered = false;
  }

  return WR_SIGNALED;
}

wait_result event_t::wait(const unsigned int msec) {
  using namespace std::chrono;

  const auto  deadline = high_resolution_clock::now() + milliseconds(msec);
  unique_lock g(m_mutex);

  while (!m_triggered) {
    if (m_cond.wait_until(g, deadline) == std::cv_status::timeout) {
      if (m_auto) {
        m_triggered = false;
      }
      return WR_TIMEOUT;
    }
  }

  if (m_auto) {
    m_triggered = false;
  }

  return WR_SIGNALED;
}

} // namespace core
} // namespace acto
