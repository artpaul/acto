#include "runtime.h"
#include "worker.h"

namespace acto {
namespace core {

worker_t::worker_t(callbacks* const slots)
  : m_slots (slots)
  , m_thread(&worker_t::execute, this)
{
  m_complete.reset();
}

worker_t::~worker_t() {
  m_active = false;
  m_event.signaled();
  m_complete.wait();
  m_thread.join();
}

void worker_t::assign(object_t* const obj, const clock_t slice) {
  assert(!m_object && obj);

  m_object = obj;
  m_start = clock();
  m_time = slice;
  // Так как поток оперирует объектом, то необходимо захватить
  // ссылку на объект, иначе объект может быть удален
  // во время обработки сообщений
  runtime_t::instance()->acquire(obj);
  // Активировать поток
  m_event.signaled();
}

void worker_t::wakeup() {
  m_event.signaled();
}

void worker_t::execute() {
  while (m_active) {
    //
    // Если данному потоку назначен объект, то необходимо
    // обрабатывать сообщения, ему поступившие
    //
    if (!process()) {
      m_slots->push_idle(this);
    }

    //
    // Ждать, пока не появится новое задание для данного потока
    //
    m_event.wait();  // Cond: (m_object != 0) || (m_active == false)
  }

  m_complete.signaled();
}

bool worker_t::check_deleting(object_t* const obj) {
  // TN: В контексте рабочего потока
  std::lock_guard<std::recursive_mutex> g(obj->cs);

  if (obj->has_messages()) {
    return false;
  }
  if (!obj->exclusive || obj->deleting) {
    obj->scheduled = false;
    m_object = nullptr;
  }
  // Если текущий объект необходимо удалить
  if (obj->deleting) {
    return true;
  }

  return false;
}

bool worker_t::process() {
  while (object_t* const obj = m_object) {
    bool released = false;

    while (auto msg = obj->select_message()) {
      assert(obj->impl);

      // Обработать сообщение.
      m_slots->handle_message(std::move(msg));

      // Проверить истечение лимита времени обработки для данного объекта.
      if (!obj->exclusive && ((clock() - m_start) > m_time)) {
        std::lock_guard<std::recursive_mutex> g(obj->cs);

        if (!obj->deleting) {
          assert(obj->impl);

          if (obj->has_messages()) {
            runtime_t::instance()->release(obj);

            m_slots->push_object(obj);

            released = true;
          } else {
            obj->scheduled = false;
          }
          m_object = nullptr;
          break;
        }
      }
    }

    if (!released && check_deleting(obj)) {
      m_slots->push_delete(obj);
    }

    //
    // Получить новый объект для обработки, если он есть в очереди
    //
    if (m_object) {
      if (m_object->exclusive) {
        return true;
      }
    } else {
      // Освободить ссылку на предыдущий объект
      if (!released) {
        runtime_t::instance()->release(obj);
      }
      /// Retrieve next object from the queue.
      if ((m_object = m_slots->pop_object())) {
        m_start = clock();
        runtime_t::instance()->acquire(m_object);
      } else {
        // Поместить текущий поток в список свободных
        return false;
      }
    }
  }
  return true;
}

} // namespace core
} // namespace acto
