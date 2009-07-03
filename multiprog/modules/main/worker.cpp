
#include <core/runtime.h>

#include "worker.h"
#include "module.h"

namespace acto {

namespace core {


/**
*/
class object_processor_t {
    // -
    object_t* volatile      m_object;
    // -
    clock_t                 m_start;
    clock_t                 m_time;
    // -
    const worker_t::Slots   m_slots;

public:
    object_processor_t(const worker_t::Slots slots)
        : m_object(NULL)
        , m_slots (slots)
        , m_start (0)
    {
    }

    void assign(object_t* const obj, const clock_t slice) {
        m_object = obj;
        m_start  = clock();
        m_time   = slice;
    }
    ///
    /// \return true  - если есть возможность обработать следующие сообщения
    ///         false - если сообщений больше нет
    bool process(worker_t* const owner) {
        while (object_t* const obj = m_object) {
            // -
            if (package_t* const package = obj->select_message()) {
                // Обработать сообщение
                m_slots.handle(package);
                // -
                delete package;

                // Проверить истечение лимита времени
                // обработки для данного объекта
                if (m_object->impl && !static_cast<base_t*>(m_object->impl)->m_thread) {
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
                        if (static_cast<base_t*>(m_object->impl)->m_thread == NULL) {
                            // -
                            obj->scheduled = false;
                            m_object = NULL;
                        }
                        else { // Если это эксклюзивный объект
                            assert(static_cast<base_t*>(m_object->impl)->m_thread == owner);
                            // -
                            if (obj->deleting) {
                                // -
                                obj->scheduled = false;
                                m_object = 0;
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
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
worker_t::worker_t(const Slots slots, thread_pool_t* const pool)
    : m_active(true)
    , m_event(true)
    , m_slots (slots)
{
    m_complete.reset();
    m_event.reset();
    m_processor.reset(new object_processor_t(slots));

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
    // 1. Назнгачить новый объект потоку
    m_processor->assign(obj, slice);
    // 2. Активировать поток, если необходимо
    if (obj)
        m_event.signaled();
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
        if (!m_processor->process(this))
            m_slots.idle(this);
        //
        // 2. Ждать, пока не появится новое задание для данного потока
        //
        m_event.wait();  // Cond: (m_object != 0) || (m_active == false)
    }
    // -
    m_complete.signaled();
}

}; // namespace core

}; // namespace acto