
#include <core/runtime.h>

#include "worker.h"
#include "module.h"

namespace acto {

namespace core {


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
worker_t::worker_t(const Slots slots, thread_pool_t* const pool)
    : m_active(true)
    , m_object(NULL)
    , m_event(true)
    , m_start(0)
    , m_time(0)
    , m_slots (slots)
{
    next = NULL;
    // -
    m_complete.reset();
    m_event.reset();

    pool->queue_task(fastdelegate::MakeDelegate(this, &worker_t::execute), 0);
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
    if (obj) {
        m_object = obj;
        m_start  = clock();
        m_time   = slice;
        // Активировать поток
        m_event.signaled();
    }
}
//-----------------------------------------------------------------------------
void worker_t::wakeup() {
    m_event.signaled();
}
//-----------------------------------------------------------------------------
void worker_t::execute(void* param) {
    while (m_active) {
        //
        // 1. Если данному потоку назначен объект, то необходимо
        //    обрабатывать сообщения, ему поступившие
        //
        if (!this->process())
            m_slots.idle(this);
        //
        // 2. Ждать, пока не появится новое задание для данного потока
        //
        m_event.wait();  // Cond: (m_object != 0) || (m_active == false)
    }
    // -
    m_complete.signaled();
}
//-----------------------------------------------------------------------------
bool worker_t::process() {
    while (object_t* const obj = m_object) {
        assert(obj != NULL);
        // -
        if (package_t* const package = obj->select_message()) {
            // Обработать сообщение
            m_slots.handle(package);
            // -
            delete package;

            // Проверить истечение лимита времени
            // обработки для данного объекта
            if (obj->impl && !static_cast<base_t*>(obj->impl)->m_thread) {
                if ((clock() - m_start) > m_time) {
                    m_slots.push(obj);
                    m_object = NULL;
                }
            }
        }
        else {
            bool deleting = false;
            // -
            {
                MutexLocker lock(obj->cs);
                // -
                if (!obj->has_messages()) {
                    // Если это динамический объект
                    if (obj->impl && static_cast<base_t*>(obj->impl)->m_thread == NULL) {
                        // -
                        obj->scheduled = false;
                        m_object = NULL;
                    }
                    else { // Если это эксклюзивный объект
                        assert(obj->impl && static_cast<base_t*>(obj->impl)->m_thread == this);
                        // -
                        if (obj->deleting) {
                            // -
                            obj->scheduled = false;
                            m_object = NULL;
                        }
                    }
                    //
                    // 2. Если текущий объект необходимо удалить
                    //
                    if (obj->deleting) {
                        assert(!obj->scheduled);
                        // -
                        runtime_t::instance()->destroy_object_body(obj);
                        // -
                        if (!obj->freeing && (obj->references == 0)) {
                            obj->freeing = true;
                            deleting     = true;
                        }
                    }
                }
            }
            // -
            if (deleting)
                m_slots.deleted(obj);
        }

        // Получить новый объект для обработки,
        // если он есть в очереди
         if (m_object == NULL) {
            m_object = m_slots.pop();
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
