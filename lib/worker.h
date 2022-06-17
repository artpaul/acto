#pragma once

#include "event.h"
#include "generics.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace acto {
namespace core {

class worker_t;
struct object_t;
struct msg_t;

/**
 * Worker thread.
 */
class worker_t : public generics::intrusive_t<worker_t> {
public:
  class callbacks {
  public:
    virtual ~callbacks() = default;

    /** Process the message. */
    virtual void handle_message(object_t* obj, std::unique_ptr<msg_t>) = 0;

    /** Schedule object to delete. */
    virtual void push_delete(object_t* const) = 0;

    /** Put itself to idle list. */
    virtual void push_idle(worker_t* const) = 0;

    /** Return object to shared queue. */
    virtual void push_object(object_t* const) = 0;

    /** Try to acquire additional job. */
    virtual object_t* pop_object() = 0;
  };

public:
  worker_t(callbacks* const slots, std::function<void()> init_cb);
  ~worker_t();

  /**
   * Assigns an object to the worker.
   */
  void assign(object_t* const obj,
              const std::chrono::steady_clock::duration slice);

  void wakeup();

private:
  void execute();

  /**
   * @return true if there are some messages in object's mailbox.
   * @return false if there are no more messages in object's mailbox.
   */
  bool process();

private:
  callbacks* const slots_;
  /// Activity flag.
  std::atomic<bool> active_{true};
  /// Current assigned object.
  object_t* object_{nullptr};

  std::chrono::steady_clock::time_point start_{};
  std::chrono::steady_clock::duration time_slice_{};

  event_t event_{true};
  std::thread thread_;
};

} // namespace core
} // namespace acto
