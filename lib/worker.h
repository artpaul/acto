#pragma once

#include "generics.h"
#include "event.h"

#include <atomic>
#include <ctime>
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

  // Поместить сообщение в очередь
  void assign(object_t* const obj, const clock_t slice);

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
  callbacks* const m_slots;
  /// Флаг активности потока
  std::atomic<bool> m_active{true};
  object_t* m_object{nullptr};

  clock_t m_start{0};
  clock_t m_time{0};

  event_t m_event{true};
  event_t m_complete;
  std::thread m_thread;
};

} // namespace core
} // namespace acto
