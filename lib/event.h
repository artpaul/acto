#pragma once

#include <memory>
#include <condition_variable>

namespace acto {
namespace core {

/** */
enum wait_result {
  // -
  WR_ERROR,
  // -
  WR_SIGNALED,
  // Превышен установленный период ожидания
  WR_TIMEOUT
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
  wait_result wait(const unsigned int msec);

private:
  const bool m_auto;
  bool m_triggered;

  std::mutex m_mutex;
  std::condition_variable m_cond;
};

} // namespace core
} // namespace acto
