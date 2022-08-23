#pragma once

#include "acto.h"
#include "worker.h"

#include <atomic>
#include <memory>
#include <thread>
#include <unordered_set>

namespace acto {
namespace core {

/**
 * Данные среды выполнения
 */
class runtime_t : public worker_t::callbacks {
  using actors_set = std::unordered_set<object_t*>;

  // Максимальное кол-во рабочих потоков в системе
  static constexpr unsigned int MAX_WORKERS = 512;

public:
  runtime_t();
  ~runtime_t();

  static runtime_t* instance();

public:
  /// Aquires reference to the object.
  unsigned long acquire(object_t* const obj) const noexcept;

  /// Sets object to deleting state and destroy object's body if there are no
  /// messages left in the inbox.
  void deconstruct_object(object_t* const object);

  ///
  void handle_message(object_t* obj, std::unique_ptr<msg_t> msg) override;

  /// Ждать уничтожения тела объекта
  void join(object_t* const obj);

  /// -
  void process_binded_actors();

  /// -
  unsigned long release(object_t* const obj);

  /// Sends the message to the specific actor.
  /// Uses the active actor as a sender.
  bool send(object_t* const target, std::unique_ptr<msg_t> msg);

  /// Sends the message to the specific actor.
  bool send_on_behalf(object_t* const target,
                      object_t* sender,
                      std::unique_ptr<msg_t> msg);

  /// Cleanups allocated resources.
  void shutdown();

  object_t* make_instance(actor_ref context,
                          const actor_thread thread_opt,
                          std::unique_ptr<actor> body);

private:
  object_t* create_actor(std::unique_ptr<actor> body,
                         const actor_thread thread_opt);

  worker_t* create_worker();

  void execute();

private:
  void push_delete(object_t* const obj) override;

  void push_idle(worker_t* const worker) override;

  object_t* pop_object() override;

  void push_object(object_t* const obj) override;

private:
  struct workers_t {
    /// Number of allocated threads.
    std::atomic<unsigned long> count{0};
    /// Number of dedicated threads.
    std::atomic<unsigned long> reserved{0};
    /// List of idle threads.
    generics::mpsc_stack_t<worker_t> idle;
  };

  /// Number of physical cores in the system.
  const unsigned long m_processors{std::thread::hardware_concurrency()};

  std::mutex mutex_;
  /// There are no more managed objects event.
  event_t no_actors_event_;
  /// Object's queue become non empty event.
  event_t queue_event_{true};
  /// There are some idle workers evetn.
  event_t idle_workers_event_{true};
  /// There are no more worker threads event.
  event_t no_workers_event_;

  /// Set of managed objects.
  actors_set actors_;
  /// Queue of objects with non empty inbox.
  generics::intrusive_queue_t<object_t> queue_;
  /// Currently allocated worker threads.
  workers_t workers_;
  /// -
  std::atomic<bool> active_{true};
  std::atomic<bool> terminating_{false};
  /// Scheduler thread.
  std::thread m_scheduler;
};

} // namespace core
} // namespace acto
