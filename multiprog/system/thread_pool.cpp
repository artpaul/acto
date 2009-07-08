
#include <cstdlib>
#include <memory>

#include <generic/intrlist.h>
#include <generic/stack.h>
#include <system/thread.h>
#include <system/event.h>

#include "thread_pool.h"

namespace acto {

/**
 */
struct thread_pool_t::thread_data_t : public core::intrusive_t< thread_data_t > {
    atomic_t                        active;
    thread_pool_t* const            owner;
    task_t                          task;
    std::auto_ptr< core::thread_t > thread;
    core::event_t                   event;
    bool                            deleting;

public:
    thread_data_t(thread_pool_t* const owner_, const task_t ut)
        : active(1)
        , owner(owner_)
        , task(ut)
        , event(true)
        , deleting(false)
    {
        next = NULL;
        // Создать поток
        thread.reset(new core::thread_t(&thread_pool_t::execute_loop, this));
    }
};


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
thread_pool_t::thread_pool_t()
    : m_count(0)
{
}
//-----------------------------------------------------------------------------
thread_pool_t::~thread_pool_t() {
    this->collect_all();

    assert(m_count == 0);
}
//-----------------------------------------------------------------------------
thread_pool_t* thread_pool_t::instance() {
    static thread_pool_t   value;

    return &value;
}
//-----------------------------------------------------------------------------
void thread_pool_t::queue_task(callback_t cb, void* param) {
    if (thread_data_t* const data = m_idles.pop()) {
        data->task = task_t(cb, param);
        data->event.signaled();
    }
    else {
        try {
            new thread_data_t(this, task_t(cb, param));
            // -
            atomic_increment(&m_count);
        }
        catch (core::thread_exception&) {
            // Не возможно создать поток
            m_tasks.push(new task_t(cb, param));
        }
    }
}

//-----------------------------------------------------------------------------
void thread_pool_t::collect_all() {
    // TN: Вызов данного метода может осуществляться из разных потоков
    generics::stack_t< thread_data_t > queue(m_idles.extract());
    // Удалить все потоки
    while (thread_data_t* const item = queue.pop())
        delete_worker(item);
}
//-----------------------------------------------------------------------------
bool thread_pool_t::delete_idle_worker(thread_data_t* const ctx) {
    thread_data_t* idle = NULL;

    {
        core::MutexLocker lock(m_cs);
        // -
        {
            // -
            if (!ctx->deleting && m_idles.front() != ctx) {
                idle = m_idles.pop();

                if (idle) {
                    if (ctx == idle)
                        m_idles.push(idle);
                    else
                        idle->deleting = true;
                }
            }
        }
    }

    if (idle)
        this->delete_worker(idle);

    return idle != NULL;
}
//-----------------------------------------------------------------------------
void thread_pool_t::delete_worker(thread_data_t* const item) {
    item->active = 0;
    // -
    if (item->thread.get() != NULL) {
        item->event.signaled();
        // Дождаться завершения системного потока
        item->thread->join();
    }
    // -
    delete item;
    // -
    atomic_decrement(&m_count);
}

//-----------------------------------------------------------------------------
void thread_pool_t::execute_loop(void* param) {
    thread_data_t* const pthis  = static_cast< thread_data_t* >(param);
    thread_pool_t* const powner = pthis->owner;

    while (pthis->active) {
        // Выполнять пользовательские задания
        while (pthis->task.callback != NULL) {
            // Вызвать обработчик
            pthis->task.callback(pthis->task.param);

            // Извлечь следующее задание из очереди, если оно имеется
            if (task_t* const task = powner->m_tasks.pop()) {
                pthis->task = *task;
                delete task;
            }
            else
                // Очистить параметры
                pthis->task = task_t(NULL, NULL);

        }

        //
        // В отстутсвии заданий поместить текущий поток в пул неиспользуемых
        //
        powner->m_idles.push(pthis);

        // Ждать пробуждения потока и параллельно обеспечить
        // удаление неиспользуемых потоков
        while (true) {
            bool timed = true;
            // Случайный разброс в перидах ожидания для того, чтобы уменьшить
            // вероятность одновременного удаления нескольких потоков
            const core::WaitResult rval = timed ? pthis->event.wait((60 + rand() % 30) * 1000) : pthis->event.wait();
            if (rval != core::WR_TIMEOUT)
                break;
            else
                timed = powner->delete_idle_worker(pthis);
        }
    }
}

} // namespace acto
