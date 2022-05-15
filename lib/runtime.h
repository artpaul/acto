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
  /// Захватить ссылку на объект
  unsigned long acquire(object_t* const obj) const noexcept;
  /// Создать экземпляр объекта, связав его с соответсвтующей реализацией
  object_t* create_actor(std::unique_ptr<actor> body, const uint32_t options);
  /// Создать контекст связи с текущим системным потоком
  void create_thread_binding();
  /// Уничтожить объект
  void deconstruct_object(object_t* const object);
  ///
  void destroy_thread_binding();
  ///
  void handle_message(std::unique_ptr<msg_t> msg) override;
  /// Ждать уничтожения тела объекта
  void join(object_t* const obj);
  /// -
  void process_binded_actors();
  /// -
  unsigned long release(object_t* const obj);
  /// Зарегистрировать новый модуль
  void register_module();

  /// Sends the message to the specific actor.
  /// Uses the active actor as a sender.
  bool send(object_t* const target, std::unique_ptr<msg_t> msg);

  /// Sends the message to the specific actor.
  bool send_on_behalf(
    object_t* const target, object_t* sender, std::unique_ptr<msg_t> msg);

  /// Завершить выполнение
  void shutdown();
  /// Начать выполнение
  void startup();

  object_t* make_instance(
    actor_ref context, const uint32_t options, std::unique_ptr<actor> body);

private:
  void push_delete(object_t* const obj) override;

  void push_idle(worker_t* const worker) override;

  object_t* pop_object() override;

  void push_object(object_t* const obj) override;

private:
  void execute();

  worker_t* create_worker();

  /**
   * @brief Processes all available messages for the given acctors.
   *
   * @param actors set of actors to process.
   * @param need_delete delete actors after process all messages.
   */
  void process_binded_actors(actors_set& actors, const bool need_delete);

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
  /// Критическая секция для доступа к полям
  std::mutex m_cs;
  ///
  event_t m_clean;
  /// Текущее множество актеров
  actors_set m_actors;
  ///
  event_t m_event{true};
  event_t m_evworker{true};
  event_t m_evnoworkers;
  /// Queue of ojbects with non empty inbox.
  generics::queue_t<object_t> m_queue;
  /// -
  workers_t m_workers;
  /// -
  std::atomic<bool> m_active{true};
  std::atomic<bool> m_terminating{false};
  /// Scheduler thread.
  std::thread m_scheduler;
};

} // namespace core
} // namespace acto
