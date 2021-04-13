#include "module.h"
#include "runtime.h"
#include "worker.h"

#include <atomic>
#include <thread>

namespace acto {
namespace core {

/// Объект, от имени которого посылается сообщение
thread_local object_t* active_actor{nullptr};

/**
 */
class main_module_t::impl : public worker_callbacks {
  // -
  using HeaderQueue = generics::queue_t< object_t >;
  // -
  using WorkerStack = generics::mpsc_stack_t< worker_t >;

  //
  struct workers_t {
    // Текущее кол-во потоков
    std::atomic<long> count{0};
    // Текущее кол-во эксклюзивных потоков
    std::atomic<long> reserved{0};
    // Свободные потоки
    WorkerStack idle;
  };

  // Максимальное кол-во рабочих потоков в системе
  static constexpr ssize_t MAX_WORKERS = 512;

public:
  impl(runtime_t* rt)
    : m_rt(rt)
  {
    m_evnoworkers.signaled();
    m_scheduler.reset(new std::thread(&impl::execute, this));
  }

  ~impl() {
    // Дождаться, когда все потоки будут удалены
    m_terminating = true;

    m_event.signaled();
    m_evnoworkers.wait();

    m_active = false;

    m_event.signaled();

    if (m_scheduler->joinable()) {
      m_scheduler->join();
    }

    assert(m_workers.count == 0 && m_workers.reserved == 0);
  }

public:
  object_t* create_actor(base_t* const body, const int options) {
    assert(body);

    object_t* const result = m_rt->create_actor(body, options);

    // Создать для актера индивидуальный поток
    if (options & acto::aoExclusive) {
      worker_t* const worker = this->create_worker();

      result->scheduled = true;
      body->thread_ = worker;

      ++m_workers.reserved;

      worker->assign(result, 0);
    }

    return result;
  }

  void destroy_object_body(base_t* const body) {
    assert(body);

    if (body->thread_) {
      --m_workers.reserved;
      body->thread_->wakeup();
    }
  }

  void handle_message(std::unique_ptr<msg_t> msg) override {
    assert(msg);
    assert(msg->target);
    assert(msg->target->impl);

    object_t* const obj = msg->target;

    try {
      assert(active_actor == nullptr);
      // TN: Данный параметр читает только функция determine_sender,
      //     которая всегда выполняется в контексте этого потока.
      active_actor = obj;

      obj->impl->consume_package(std::move(msg));

      active_actor = nullptr;
    } catch (...) {
      active_actor = nullptr;
    }

    if (obj->impl->terminating_) {
      m_rt->deconstruct_object(obj);
    }
  }

  void send_message(std::unique_ptr<msg_t> msg) {
    assert(msg);
    assert(msg->target);

    auto target = msg->target;

    {
      std::lock_guard<std::recursive_mutex> g(target->cs);

      // Can not send messages to deleting object.
      if (target->deleting) {
          return;
      }
      assert(target->impl);
      // 1. Поставить сообщение в очередь объекта
      target->enqueue(std::move(msg));
      // 2. Подобрать для него необходимый поток
      if (target->binded) {
        return;
      }
      // Wakeup object's thread if the target has
      // dedicated thread for message processing.
      if (worker_t* const thread = target->impl->thread_) {
        thread->wakeup();
      } else if (!target->scheduled) {
        target->scheduled = true;
        push_object(target);
      }
    }
  }

  void push_delete(object_t* const obj) override {
    m_rt->deconstruct_object(obj);
  }

  void push_idle(worker_t* const worker) override {
    assert(worker);

    m_workers.idle.push(worker);
    m_evworker.signaled();
  }

  object_t* pop_object() override {
    return m_queue.pop();
  }

  void push_object(object_t* const obj) override {
    m_queue.push(obj);
    m_event.signaled();
  }

private:
  void execute() {
    int newWorkerTimeout = 2;
    int lastCleanupTime = clock();

    while (m_active) {
      while(!m_queue.empty()) {
        dispatch_to_worker(newWorkerTimeout);
        yield();
      }

      if (m_terminating || (m_event.wait(60 * 1000) == WR_TIMEOUT)) {
        generics::stack_t<worker_t> queue(m_workers.idle.extract());

        // Удалить все потоки
        while (worker_t* const item = queue.pop()) {
          delete_worker(item);
        }

        yield();
      } else if ((clock() - lastCleanupTime) > (60 * CLOCKS_PER_SEC)) {
        if (worker_t* const item = m_workers.idle.pop()) {
          delete_worker(item);
        }

        lastCleanupTime = clock();
      }
    }
  }

  void delete_worker(worker_t* const item) {
    delete item;
    // Уведомить, что удалены все вычислительные потоки
    if (--m_workers.count == 0) {
      m_evnoworkers.signaled();
    }
  }

  worker_t* create_worker() {
    worker_t* const result = new core::worker_t(this);

    if (++m_workers.count == 1) {
      m_evnoworkers.reset();
    }

    return result;
  }

  void dispatch_to_worker(int& wait_timeout) {
    // Прежде чем извлекать объект из очереди, необходимо проверить,
    // что есть вычислительные ресурсы для его обработки
    worker_t* worker = m_workers.idle.pop();

    if (!worker) {
      // Если текущее количество потоков меньше оптимального,
      // то создать новый поток
      if (m_workers.count < (m_workers.reserved + (m_processors << 1))) {
        worker = create_worker();
      } else {
        // Подождать некоторое время осовобождения какого-нибудь потока
        const wait_result result = m_evworker.wait(wait_timeout * 1000);

        worker = m_workers.idle.pop();

        if (!worker && (result == WR_TIMEOUT)) {
          wait_timeout += 2;

          if (m_workers.count < MAX_WORKERS) {
            worker = create_worker();
          } else {
            m_evworker.wait();
          }
        } else {
          if (wait_timeout > 2) {
            wait_timeout -= 2;
          }
        }
      }
    }

    if (worker) {
      if (object_t* const obj = m_queue.pop()) {
        worker->assign(obj, (CLOCKS_PER_SEC >> 2));
      } else {
        m_workers.idle.push(worker);
      }
    }
  }

private:
  runtime_t* const m_rt;
  ///
  event_t m_event{true};
  event_t m_evworker{true};
  event_t m_evnoworkers;
  /// Количество физических процессоров (ядер) в системе
  long m_processors{NumberOfProcessors()};
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


///////////////////////////////////////////////////////////////////////////////

main_module_t::main_module_t() = default;

main_module_t::~main_module_t() = default;

object_t* main_module_t::determine_sender() {
  return active_actor;
}

void main_module_t::destroy_object_body(base_t* const body) {
  m_pimpl->destroy_object_body(body);
}

void main_module_t::handle_message(std::unique_ptr<msg_t> msg) {
  m_pimpl->handle_message(std::move(msg));
}

void main_module_t::send_message(std::unique_ptr<msg_t> msg) {
  m_pimpl->send_message(std::move(msg));
}

void main_module_t::shutdown(event_t& event) {
  m_pimpl.reset(nullptr);
  event.signaled();
}

void main_module_t::startup(runtime_t* rt) {
  m_pimpl.reset(new impl(rt));
}

object_t* main_module_t::create_actor(base_t* const body, const int options) {
  return m_pimpl->create_actor(body, options);
}

} // namespace core
} // namespace acto
