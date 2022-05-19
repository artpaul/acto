#include "runtime.h"
#include "generics.h"
#include "worker.h"

namespace acto {
namespace core {
namespace {

/**
 * Local context of the current thread.
 */
struct binding_context_t {
  /// Pointer to the active actor is using to
  /// implicitly determine a sender for a message.
  object_t* active_actor{nullptr};

  /// Set of actors binded to the current thread.
  std::unordered_set<object_t*> actors;

  /// It is a worker thread created by the library.
  bool is_worker_thread{false};

  ~binding_context_t() {
    // Mark all actors as deleting to prevent message loops.
    for (auto* obj : actors) {
      runtime_t::instance()->deconstruct_object(obj);
    }

    process_actors(true);
  }

  /**
   * Processes all available messages for the acctors.
   *
   * @param need_delete delete actors after process all messages.
   */
  void process_actors(const bool need_delete) {
    auto runtime = runtime_t::instance();
    // TODO: - switch between active actors to balance message processing.
    //       - detect message loop.
    for (auto ai = actors.cbegin(); ai != actors.cend(); ++ai) {
      while (auto msg = (*ai)->select_message()) {
        runtime->handle_message(std::move(msg));
      }
      if (need_delete) {
        runtime->deconstruct_object(*ai);
      }
    }

    if (need_delete) {
      for (auto ai = actors.cbegin(); ai != actors.cend(); ++ai) {
        runtime->release(*ai);
      }

      actors.clear();
    }
  }
};

static thread_local binding_context_t thread_context;

} // namespace

runtime_t::runtime_t()
  : m_scheduler(&runtime_t::execute, this) {
  no_actors_event_.signaled();
  no_workers_event_.signaled();
}

runtime_t::~runtime_t() {
  m_terminating = true;
  // Дождаться, когда все потоки будут удалены
  queue_event_.signaled();
  no_workers_event_.wait();

  m_active = false;

  queue_event_.signaled();
  // Stop scheduler's thread.
  if (m_scheduler.joinable()) {
    m_scheduler.join();
  }

  assert(workers_.count == 0 && workers_.reserved == 0);
}

runtime_t* runtime_t::instance() {
  static runtime_t value;
  return &value;
}

unsigned long runtime_t::acquire(object_t* const obj) const noexcept {
  assert(bool(obj) && obj->references);
  return ++obj->references;
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
        --workers_.reserved;
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
    std::lock_guard<std::mutex> g(mutex_);

    actors_.erase(obj);

    if (actors_.empty()) {
      no_actors_event_.signaled();
    }
  }
  // There are no more references to the object,
  // so delete it.
  delete obj;
}

void runtime_t::handle_message(std::unique_ptr<msg_t> msg) {
  assert(msg);
  assert(msg->target);
  assert(msg->target->impl);

  object_t* const obj = msg->target;

  try {
    assert(thread_context.active_actor == nullptr);

    thread_context.active_actor = obj;

    obj->impl->consume_package(std::move(msg));

    thread_context.active_actor = nullptr;
  } catch (...) {
    thread_context.active_actor = nullptr;
  }
  if (obj->impl->terminating_) {
    deconstruct_object(obj);
  }
}

void runtime_t::join(object_t* const obj) {
  if (thread_context.active_actor == obj) {
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
  thread_context.process_actors(false);
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
  return send_on_behalf(target, thread_context.active_actor, std::move(msg));
}

bool runtime_t::send_on_behalf(object_t* const target,
                               object_t* const sender,
                               std::unique_ptr<msg_t> msg) {
  assert(msg);
  assert(target);

  {
    std::lock_guard<std::recursive_mutex> g(target->cs);
    // Cannot send messages to deleting object.
    if (target->deleting) {
      return false;
    } else {
      // Acquire reference to a sender.
      if (sender) {
        msg->sender = sender;
        acquire(sender);
      }
      // Acquire reference to the target actor.
      msg->target = target;
      acquire(target);
    }
    // Enqueue the message.
    target->enqueue(std::move(msg));
    // Do not try to select a worker thread for a binded actor.
    if (target->binded) {
      target->scheduled = true;
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
  // Process all messages for binded actors and stop them.
  thread_context.process_actors(true);

  // Process shared actors.
  {
    actors_set actors;

    {
      std::lock_guard<std::mutex> g(mutex_);
      actors = actors_;
    }

    for (auto ai = actors.cbegin(); ai != actors.cend(); ++ai) {
      deconstruct_object(*ai);
    }

    no_actors_event_.wait();
  }

  assert(actors_.empty());
}

object_t* runtime_t::make_instance(actor_ref context,
                                   const actor_thread thread_opt,
                                   std::unique_ptr<actor> body) {
  assert(body);
  // Create core object.
  core::object_t* const result = create_actor(std::move(body), thread_opt);

  if (result) {
    result->impl->context_ = std::move(context);
    result->impl->self_ = actor_ref(result, true);
    result->impl->bootstrap();
  }

  return result;
}

object_t* runtime_t::create_actor(std::unique_ptr<actor> body,
                                  const actor_thread thread_opt) {
  object_t* const result = new core::object_t(thread_opt, std::move(body));
  // Bind actor to the current thread if the thread did not created by the
  // library.
  if (thread_opt == actor_thread::bind && !thread_context.is_worker_thread) {
    result->references += 1;
    thread_context.actors.insert(result);
  } else {
    {
      std::lock_guard<std::mutex> g(mutex_);

      actors_.insert(result);

      no_actors_event_.reset();
    }
    // Create dedicated thread for the actor if necessary.
    if (thread_opt == actor_thread::exclusive) {
      worker_t* const worker = create_worker();

      result->scheduled = true;
      result->thread = worker;

      ++workers_.reserved;

      worker->assign(result, std::chrono::steady_clock::duration());
    }
  }

  return result;
}

worker_t* runtime_t::create_worker() {
  worker_t* const result =
    new core::worker_t(this, [] { thread_context.is_worker_thread = true; });

  if (++workers_.count == 1) {
    no_workers_event_.reset();
  }

  return result;
}

void runtime_t::execute() {
  auto last_cleanup_time = std::chrono::steady_clock::now();
  auto new_worker_timeout = std::chrono::seconds(2);

  auto delete_worker = [this](worker_t* const item) {
    delete item;
    // Уведомить, что удалены все вычислительные потоки
    if (--workers_.count == 0) {
      no_workers_event_.signaled();
    }
  };

  while (m_active) {
    while (!queue_.empty()) {
      // Прежде чем извлекать объект из очереди, необходимо проверить,
      // что есть вычислительные ресурсы для его обработки
      worker_t* worker = workers_.idle.pop();

      if (!worker) {
        // Если текущее количество потоков меньше оптимального,
        // то создать новый поток
        if (workers_.count < (workers_.reserved + (m_processors << 1))) {
          worker = create_worker();
        } else {
          // Подождать некоторое время осовобождения какого-нибудь потока
          const wait_result result = idle_workers_event_.wait(new_worker_timeout);

          worker = workers_.idle.pop();

          if (!worker && (result == wait_result::timeout)) {
            new_worker_timeout += std::chrono::seconds(2);

            if (workers_.count < MAX_WORKERS) {
              worker = create_worker();
            } else {
              idle_workers_event_.wait();
            }
          } else if (new_worker_timeout > std::chrono::seconds(2)) {
            new_worker_timeout -= std::chrono::seconds(2);
          }
        }
      }

      if (worker) {
        if (object_t* const obj = queue_.pop()) {
          worker->assign(obj, std::chrono::milliseconds(500));
        } else {
          workers_.idle.push(worker);
        }
      }
    }

    if (m_terminating ||
        (queue_event_.wait(std::chrono::seconds(60)) == wait_result::timeout))
    {
      generics::stack_t<worker_t> queue(workers_.idle.extract());

      // Удалить все потоки
      while (worker_t* const item = queue.pop()) {
        delete_worker(item);
      }
    } else if ((std::chrono::steady_clock::now() - last_cleanup_time) >
               std::chrono::seconds(60))
    {
      if (worker_t* const item = workers_.idle.pop()) {
        delete_worker(item);
      }

      last_cleanup_time = std::chrono::steady_clock::now();
    }
  }
}

void runtime_t::push_delete(object_t* const obj) {
  deconstruct_object(obj);
}

void runtime_t::push_idle(worker_t* const worker) {
  assert(worker);

  workers_.idle.push(worker);
  idle_workers_event_.signaled();
}

object_t* runtime_t::pop_object() {
  return queue_.pop();
}

void runtime_t::push_object(object_t* const obj) {
  queue_.push(obj);
  queue_event_.signaled();
}

} // namespace core
} // namespace acto
