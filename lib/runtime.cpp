#include "runtime.h"
#include "generics.h"
#include "worker.h"

namespace acto {
namespace core {
namespace {

/** Контекс потока */
struct binding_context_t {
  // Список актеров ассоциированных
  // только с текущим потоком
  std::unordered_set<object_t*> actors;
  // Счетчик инициализаций
  std::atomic<long> counter;
};

/// Pointer to the active actor is using to
/// implicitly determine a sender for a message.
static thread_local object_t* active_actor{nullptr};
/// -
static thread_local std::unique_ptr<binding_context_t> thread_context{nullptr};

} // namespace

runtime_t::runtime_t()
  : m_scheduler(&runtime_t::execute, this) {
  m_clean.signaled();
  m_evnoworkers.signaled();
}

runtime_t::~runtime_t() {
  m_terminating = true;
  // Дождаться, когда все потоки будут удалены
  m_event.signaled();
  m_evnoworkers.wait();

  m_active = false;

  m_event.signaled();
  // Stop scheduler's thread.
  if (m_scheduler.joinable()) {
    m_scheduler.join();
  }

  assert(m_workers.count == 0 && m_workers.reserved == 0);
}

runtime_t* runtime_t::instance() {
  static runtime_t value;
  return &value;
}

unsigned long runtime_t::acquire(object_t* const obj) const noexcept {
  assert(bool(obj) && obj->references);
  return ++obj->references;
}

object_t* runtime_t::create_actor(actor* const body, const int options) {
  assert(body);

  object_t* const result = new core::object_t(body);

  result->references = 1;
  // Зарегистрировать объект в системе
  if (options & acto::aoBindToThread) {
    result->references += 1;
    result->binded = true;

    thread_context->actors.insert(result);
  } else {
    std::lock_guard<std::mutex> g(m_cs);

    result->exclusive = options & acto::aoExclusive;

    m_actors.insert(result);

    m_clean.reset();
  }
  // Create dedicated thread for the actor if necessary.
  if (options & acto::aoExclusive) {
    worker_t* const worker = this->create_worker();

    result->scheduled = true;
    result->thread = worker;

    ++m_workers.reserved;

    worker->assign(result, std::chrono::steady_clock::duration());
  }

  return result;
}

void runtime_t::create_thread_binding() {
  if (thread_context) {
    ++thread_context->counter;
  } else {
    thread_context.reset(new binding_context_t);
    thread_context->counter = 1;
  }
}

void runtime_t::deconstruct_object(object_t* const obj) {
  assert(obj);

  {
    std::lock_guard<std::recursive_mutex> g(obj->cs);
    // Return from a recursive call
    // during deletion of the actor's body.
    if (obj->unimpl) {
      return;
    }
    obj->deleting = true;
    // The object still has some messages in the mailbox.
    if (obj->scheduled) {
      return;
    } else {
      assert(!obj->has_messages());
    }
    if (obj->impl) {
      if (obj->thread) {
        --m_workers.reserved;
        obj->thread->wakeup();
      }

      obj->unimpl = true;
      delete obj->impl, obj->impl = nullptr;
      obj->unimpl = false;

      if (obj->waiters) {
        for (object_t::waiter_t* it = obj->waiters; it != nullptr;) {
          // TN: Необходимо читать значение следующего указателя
          //     заранее, так как пробуждение ждущего потока
          //     приведет к удалению текущего узла списка
          object_t::waiter_t* next = it->next;
          it->event.signaled();
          it = next;
        }

        obj->waiters = nullptr;
      }
    }
    // Cannot delete the object if there are still some references to it.
    if (obj->references) {
      return;
    }
  }
  // Remove object from the global registry.
  if (!obj->binded) {
    std::lock_guard<std::mutex> g(m_cs);

    m_actors.erase(obj);

    if (m_actors.empty()) {
      m_clean.signaled();
    }
  }
  // There are no more references to the object,
  // so delete it.
  delete obj;
}

void runtime_t::destroy_thread_binding() {
  if (thread_context) {
    if (--thread_context->counter == 0) {
      this->process_binded_actors(thread_context->actors, true);
      thread_context.reset();
    }
  }
}

void runtime_t::handle_message(std::unique_ptr<msg_t> msg) {
  assert(msg);
  assert(msg->target);
  assert(msg->target->impl);

  object_t* const obj = msg->target;

  try {
    assert(active_actor == nullptr);

    active_actor = obj;

    obj->impl->consume_package(std::move(msg));

    active_actor = nullptr;
  } catch (...) {
    active_actor = nullptr;
  }
  if (obj->impl->terminating_) {
    deconstruct_object(obj);
  }
}

void runtime_t::join(object_t* const obj) {
  if (active_actor == obj) {
    return;
  }

  object_t::waiter_t node;

  {
    std::lock_guard<std::recursive_mutex> g(obj->cs);

    if (obj->impl) {
      node.event.reset();
      node.next = obj->waiters;
      obj->waiters = &node;
    } else {
      return;
    }
  }

  node.event.wait();
}

void runtime_t::process_binded_actors() {
  assert(thread_context);

  process_binded_actors(thread_context->actors, false);
}

unsigned long runtime_t::release(object_t* const obj) {
  assert(obj);
  assert(obj->references);

  const long result = --obj->references;

  if (result == 0) {
    deconstruct_object(obj);
  }

  return result;
}

bool runtime_t::send(object_t* const target, std::unique_ptr<msg_t> msg) {
  assert(msg);
  assert(target);

  {
    std::lock_guard<std::recursive_mutex> g(target->cs);
    // Can not send messages to deleting object.
    if (target->deleting) {
      return false;
    } else {
      // Acquire reference to a sender.
      if (active_actor) {
        msg->sender = active_actor;
        acquire(active_actor);
      }
      // Acquire reference to the target actor.
      msg->target = target;
      acquire(target);
    }
    // Enqueue the message.
    target->enqueue(std::move(msg));
    // Do not try to select a worker thread for a binded actor.
    if (target->binded) {
      return true;
    }
    // Wakeup object's thread if the target has
    // a dedicated thread for message processing.
    if (worker_t* const thread = target->thread) {
      thread->wakeup();
    } else if (!target->scheduled) {
      target->scheduled = true;
      push_object(target);
    }
  }

  return true;
}

void runtime_t::shutdown() {
  this->destroy_thread_binding();

  // 1. Инициировать процедуру удаления для всех оставшихся объектов
  {
    std::lock_guard<std::mutex> g(m_cs);

    actors_set temporary(m_actors);

    for (auto ti = temporary.cbegin(); ti != temporary.cend(); ++ti) {
      deconstruct_object(*ti);
    }
  }

  m_clean.wait();

  assert(m_actors.size() == 0);
}

void runtime_t::startup() {
  create_thread_binding();
}

void runtime_t::process_binded_actors(
  actors_set& actors, const bool need_delete) {
  for (auto ai = actors.cbegin(); ai != actors.cend(); ++ai) {
    while (auto msg = (*ai)->select_message()) {
      handle_message(std::move(msg));
    }
    if (need_delete) {
      deconstruct_object(*ai);
    }
  }

  if (need_delete) {
    for (auto ai = actors.cbegin(); ai != actors.cend(); ++ai) {
      release(*ai);
    }

    actors.clear();
  }
}

object_t* runtime_t::make_instance(
  actor_ref context, const int options, actor* body) {
  assert(body);
  // Создать объект ядра (счетчик ссылок увеличивается автоматически)
  core::object_t* const result = create_actor(body, options);

  if (result) {
    body->context_ = std::move(context);
    body->self_ = actor_ref(result, true);
    body->bootstrap();
  } else {
    delete body;
  }

  return result;
}

void runtime_t::push_delete(object_t* const obj) {
  deconstruct_object(obj);
}

void runtime_t::push_idle(worker_t* const worker) {
  assert(worker);

  m_workers.idle.push(worker);
  m_evworker.signaled();
}

object_t* runtime_t::pop_object() {
  return m_queue.pop();
}

void runtime_t::push_object(object_t* const obj) {
  m_queue.push(obj);
  m_event.signaled();
}

worker_t* runtime_t::create_worker() {
  worker_t* const result = new core::worker_t(this);

  if (++m_workers.count == 1) {
    m_evnoworkers.reset();
  }

  return result;
}

void runtime_t::execute() {
  auto last_cleanup_time = std::chrono::steady_clock::now();
  auto new_worker_timeout = std::chrono::seconds(2);

  auto delete_worker = [this](worker_t* const item) {
    delete item;
    // Уведомить, что удалены все вычислительные потоки
    if (--m_workers.count == 0) {
      m_evnoworkers.signaled();
    }
  };

  while (m_active) {
    while (!m_queue.empty()) {
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
          const wait_result result = m_evworker.wait(new_worker_timeout);

          worker = m_workers.idle.pop();

          if (!worker && (result == wait_result::timeout)) {
            new_worker_timeout += std::chrono::seconds(2);

            if (m_workers.count < MAX_WORKERS) {
              worker = create_worker();
            } else {
              m_evworker.wait();
            }
          } else if (new_worker_timeout > std::chrono::seconds(2)) {
            new_worker_timeout -= std::chrono::seconds(2);
          }
        }
      }

      if (worker) {
        if (object_t* const obj = m_queue.pop()) {
          worker->assign(obj, std::chrono::milliseconds(500));
        } else {
          m_workers.idle.push(worker);
        }
      }
    }

    if (m_terminating ||
        (m_event.wait(std::chrono::seconds(60)) == wait_result::timeout))
    {
      generics::stack_t<worker_t> queue(m_workers.idle.extract());

      // Удалить все потоки
      while (worker_t* const item = queue.pop()) {
        delete_worker(item);
      }
    } else if ((std::chrono::steady_clock::now() - last_cleanup_time) >
               std::chrono::seconds(60))
    {
      if (worker_t* const item = m_workers.idle.pop()) {
        delete_worker(item);
      }

      last_cleanup_time = std::chrono::steady_clock::now();
    }
  }
}

object_t* make_instance(actor_ref context, const int options, actor* body) {
  return runtime_t::instance()->make_instance(
    std::move(context), options, body);
}

} // namespace core
} // namespace acto
