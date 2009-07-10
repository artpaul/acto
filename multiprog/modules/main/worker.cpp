
#include <core/runtime.h>

#include "worker.h"
#include "module.h"

namespace acto {

namespace core {

///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
worker_t::worker_t(worker_callback_i* const slots, thread_pool_t* const pool)
    : m_active(true)
    , m_object(NULL)
    , m_start (0)
    , m_time  (0)
    , m_event(true)
    , m_slots (slots)
{
    next = NULL;
    // -
    m_complete.reset();

    pool->queue_task(&worker_t::execute, this);
}
//-----------------------------------------------------------------------------
worker_t::~worker_t() {
    // 1.
    m_active = false;
    // 2.
    m_event.signaled();
    // 3. Дождаться завершения потока
    m_complete.wait();
}
//-----------------------------------------------------------------------------
void worker_t::assign(object_t* const obj, const clock_t slice) {
    assert(m_object == NULL && obj != NULL);

    m_object = obj;
    m_start  = clock();
    m_time   = slice;
    // Активировать поток
    m_event.signaled();
}
//-----------------------------------------------------------------------------
void worker_t::wakeup() {
    m_event.signaled();
}
//-----------------------------------------------------------------------------
void worker_t::execute(void* param) {
    worker_t* const pthis = static_cast< worker_t* >(param);

    while (pthis->m_active) {
        //
        // 1. Если данному потоку назначен объект, то необходимо
        //    обрабатывать сообщения, ему поступившие
        //
        if (!pthis->process())
            pthis->m_slots->push_idle(pthis);
        //
        // 2. Ждать, пока не появится новое задание для данного потока
        //
        pthis->m_event.wait();  // Cond: (m_object != 0) || (m_active == false)
    }
    // -
    pthis->m_complete.signaled();
}
//-----------------------------------------------------------------------------
bool worker_t::check_deleting(object_t* const obj) {
    // TN: В контексте рабочего потока
    MutexLocker lock(obj->cs);
    // -
    if (!obj->has_messages()) {
        // -
        if (!obj->exclusive || obj->deleting) {
            obj->scheduled = false;
            m_object       = NULL;
        }
        // Если текущий объект необходимо удалить
        if (obj->deleting)
            return true;
    }
    return false;
}
//-----------------------------------------------------------------------------
bool worker_t::process() {
    while (object_t* const obj = m_object) {
        assert(obj != NULL);
        // -
        while (package_t* const package = obj->select_message()) {
            assert(obj->impl);

            // Обработать сообщение
            m_slots->handle_message(package);

            // Проверить истечение лимита времени
            // обработки для данного объекта
            if (obj->impl && !obj->deleting && !obj->exclusive) {
                if ((clock() - m_start) > m_time) {
                    m_slots->push_object(obj);
                    m_object = NULL;
                    break;
                }
            }
        }

        // -
        if (check_deleting(obj))
            m_slots->push_delete(obj);

        // Получить новый объект для обработки,
        // если он есть в очереди
        if (m_object == NULL) {
            m_object = m_slots->pop_object();
            // -
            if (m_object)
                m_start = clock();
            else {
                // Поместить текущий поток в список свободных
                return false;
            }
        }
    }
    return true;
}

}; // namespace core

}; // namespace acto
