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
class runtime_t : public worker_callbacks {
  // -
  using HeaderQueue = generics::queue_t< object_t >;
  // -
  using WorkerStack = generics::mpsc_stack_t< worker_t >;

  //
  struct workers_t {
    // Текущее кол-во потоков
    std::atomic<unsigned long> count{0};
    // Текущее кол-во эксклюзивных потоков
    std::atomic<unsigned long> reserved{0};
    // Свободные потоки
    WorkerStack idle;
  };

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
  void send_message(std::unique_ptr<msg_t> msg);
  void destroy_object_body(object_t* obj);

  void execute();

  void delete_worker(worker_t* const item);

  worker_t* create_worker();

  void dispatch_to_worker(int& wait_timeout);

  void process_binded_actors(std::set<object_t*>& actors, const bool need_delete);

private:
  /// Тип множества актеров
  using actors = std::set<object_t*>;

  /// Количество физических процессоров (ядер) в системе
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
  /// Очередь объектов, которым пришли сообщения
  HeaderQueue m_queue;
  /// Экземпляр системного потока
  std::unique_ptr<std::thread> m_scheduler;
  /// -
  workers_t m_workers;
  /// -
  std::atomic<bool> m_active{true};
  std::atomic<bool> m_terminating{false};
};

} // namepsace core
} // namespace acto
