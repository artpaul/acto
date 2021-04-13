#include "generics.h"
#include "runtime.h"
#include "worker.h"

namespace acto {
namespace core {

/** Контекс потока */
struct binding_context_t {
  // Список актеров ассоциированных
  // только с текущим потоком
  std::set<object_t*> actors;
  // Счетчик инициализаций
  std::atomic<long> counter;
};

/// Объект, от имени которого посылается сообщение
static thread_local object_t* active_actor{nullptr};
/// -
static thread_local std::unique_ptr<binding_context_t> thread_context{nullptr};

runtime_t::runtime_t() {
  m_clean.signaled();
  m_evnoworkers.signaled();
  m_scheduler.reset(new std::thread(&runtime_t::execute, this));
}

runtime_t::~runtime_t() {
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

runtime_t* runtime_t::instance() {
  static runtime_t value;
  return &value;
}

unsigned long runtime_t::acquire(object_t* const obj) {
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

void runtime_t::create_thread_binding() {
  if (thread_context) {
    ++thread_context->counter;
  } else {
    thread_context.reset(new binding_context_t);
    thread_context->counter = 1;
  }
}

void runtime_t::deconstruct_object(object_t* const obj) {
  assert(obj != 0);

  bool deleting = false;
  {
    std::lock_guard<std::recursive_mutex> g(obj->cs);
    // 1. Если объект уже находится в состоянии "осовобождаемы",
    //    то он будет неминуемо удален и более ничего делать не нежно
    if (obj->freeing) {
      return;
    }
    // 2. Перевести объект в состояние "удаляемый", что в частности
    //    ведет к невозможности послать сообщения данному объекту
    obj->deleting = true;
    // 3.
    if (!obj->scheduled && !obj->unimpl) {
      assert(!obj->has_messages());

      if (obj->impl) {
        destroy_object_body(obj);
      }
      if (!obj->freeing && (0 == obj->references)) {
        obj->freeing = true;
        deleting = true;
      }
    }
  }

  if (deleting) {
    // Удалить регистрацию объекта
    if (!obj->binded) {
      std::lock_guard<std::mutex> g(m_cs);

      m_actors.erase(obj);

      if (m_actors.size() == 0) {
        m_clean.signaled();
      }
    }

    delete obj;
  }
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
    // TN: Данный параметр читает только функция determine_sender,
    //     которая всегда выполняется в контексте этого потока.
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

  this->process_binded_actors(thread_context->actors, false);
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

void runtime_t::send(object_t* const target, std::unique_ptr<msg_t> msg) {
  assert(msg);
  assert(target);
  assert(target->impl);

  msg->sender = active_actor;
  msg->target = target;

  acquire(target);

  if (msg->sender) {
    acquire(msg->sender);
  }

  send_message(std::move(msg));
}

void runtime_t::shutdown() {
  this->destroy_thread_binding();

  // 1. Инициировать процедуру удаления для всех оставшихся объектов
  {
    std::lock_guard<std::mutex> g(m_cs);

    actors temporary(m_actors);

    for (actors::iterator i = temporary.begin(); i != temporary.end(); ++i) {
      deconstruct_object(*i);
    }
  }

  m_clean.wait();

  assert(m_actors.size() == 0);
}

void runtime_t::startup() {
  this->create_thread_binding();
}

void runtime_t::process_binded_actors(std::set<object_t*>& actors, const bool need_delete) {
  for (std::set<object_t*>::iterator i = actors.begin(); i != actors.end(); ++i) {
    object_t* const actor = *i;

    while (auto msg = actor->select_message()) {
      this->handle_message(std::move(msg));
    }
    if (need_delete) {
      this->deconstruct_object(actor);
    }
  }

  if (need_delete) {
    for (std::set<object_t*>::iterator i = actors.begin(); i != actors.end(); ++i) {
      this->release(*i);
    }

    actors.clear();
  }
}

object_t* runtime_t::make_instance(actor_ref context, const int options, actor* body) {
  assert(body);
  // Создать объект ядра (счетчик ссылок увеличивается автоматически)
  core::object_t* const result = create_actor(body, options);

  if (result) {
    body->context_ = std::move(context);
    body->self_ = actor_ref(result);
    body->bootstrap();
  } else {
      delete body;
  }

  return result;
}


/// Отправить сообщение соответствующему объекту
void runtime_t::send_message(std::unique_ptr<msg_t> msg) {
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

void runtime_t::destroy_object_body(object_t* obj) {
  assert(obj);
  assert(!obj->unimpl && obj->impl);

  // TN: Эта процедура всегда должна вызываться внутри блокировки
  //     полей объекта, если ссылкой на объект могут владеть
  //     два и более потока

  {
    if (obj->impl->thread_) {
      --m_workers.reserved;
      obj->impl->thread_->wakeup();
    }
  }

  obj->unimpl = true;
  delete obj->impl, obj->impl = nullptr;
  obj->unimpl = false;

  if (obj->waiters) {
    for (object_t::waiter_t* it = obj->waiters; it != nullptr; ) {
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

void runtime_t::execute() {
  int newWorkerTimeout = 2;
  clock_t lastCleanupTime = clock();

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

void runtime_t::delete_worker(worker_t* const item) {
  delete item;
  // Уведомить, что удалены все вычислительные потоки
  if (--m_workers.count == 0) {
    m_evnoworkers.signaled();
  }
}

worker_t* runtime_t::create_worker() {
  worker_t* const result = new core::worker_t(this);

  if (++m_workers.count == 1) {
    m_evnoworkers.reset();
  }

  return result;
}

void runtime_t::dispatch_to_worker(int& wait_timeout) {
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

object_t* make_instance(actor_ref context, const int options, actor* body) {
  return runtime_t::instance()->make_instance(std::move(context), options, body);
}

} // namespace core
} // namespace acto
