#pragma once

#include "generics.h"
#include "event.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace acto {
namespace core {

class  worker_t;
struct object_t;
struct msg_t;

/**
 * Системный поток
 */
class worker_t : public generics::intrusive_t<worker_t> {
public:
  class callbacks {
  public:
    virtual ~callbacks() = default;

    virtual void handle_message(std::unique_ptr<msg_t>) = 0;
    virtual void push_delete(object_t* const) = 0;
    virtual void push_idle(worker_t* const) = 0;
    virtual void push_object(object_t* const) = 0;
    virtual object_t* pop_object() = 0;
  };

public:
  worker_t(callbacks* const slots);
  ~worker_t();

  /**
   * Assigns the object to the worker.
   */
  void assign(object_t* const obj, const std::chrono::steady_clock::duration slice);

  void wakeup();

private:
  void execute();

  ///
  bool check_deleting(object_t* const obj);

  ///
  /// \return true  - если есть возможность обработать следующие сообщения
  ///         false - если сообщений больше нет
  bool process();

private:
  callbacks* const slots_;
  /// Флаг активности потока
  std::atomic<bool> active_{true};
  /// Current assigned object.
  object_t* object_{nullptr};

  std::chrono::steady_clock::time_point start_{};
  std::chrono::steady_clock::duration time_{};

  event_t event_{true};
  event_t complete_;
  std::thread thread_;
};

} // namespace core
} // namespace acto
