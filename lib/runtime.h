#pragma once

#include "acto.h"
#include "worker.h"

#include <atomic>
#include <set>
#include <thread>

namespace acto {
namespace core {

/**
 * Данные среды выполнения
 */
class runtime_t : public worker_t::callbacks {
  // Максимальное кол-во рабочих потоков в системе
  static constexpr ssize_t MAX_WORKERS = 512;

public:
  runtime_t();
  ~runtime_t();

  static runtime_t* instance();

public:
  /// Захватить ссылку на объект
  unsigned long acquire(object_t* const obj);
  /// Создать экземпляр объекта, связав его с соответсвтующей реализацией
  object_t* create_actor(actor* const body, const int options);
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
  /// Послать сообщение указанному объекту
  void send(object_t* const target, std::unique_ptr<msg_t> msg);
  /// Завершить выполнение
  void shutdown();
  /// Начать выполнение
  void startup();

  object_t* make_instance(actor_ref context, const int options, actor* body);

private:
  void push_delete(object_t* const obj) override;

  void push_idle(worker_t* const worker) override;

  object_t* pop_object() override;

  void push_object(object_t* const obj) override;

private:
  void destroy_object_body(object_t* obj);

  void execute();

  worker_t* create_worker();

  void process_binded_actors(std::set<object_t*>& actors, const bool need_delete);

private:
  using actors = std::set<object_t*>;

  struct workers_t {
    /// Number of allocated threads.
    std::atomic<unsigned long> count{0};
    /// Number of dedicated threads.
    std::atomic<unsigned long> reserved{0};
    /// List of idle threads.
    generics::mpsc_stack_t<worker_t> idle;
  };


  /// Number of physical cores in the system.
  const unsigned long m_processors{NumberOfProcessors()};
  /// Критическая секция для доступа к полям
  std::mutex m_cs;
  ///
  event_t m_clean;
  /// Текущее множество актеров
  actors m_actors;
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

} // namepsace core
} // namespace acto
