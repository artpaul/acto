
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
class thread_worker_t : public core::intrusive_t< thread_worker_t > {
    friend class thread_pool_t;

    typedef fastdelegate::FastDelegate< void (void*) >  callback_t;

    callback_t                      m_callback;
    core::event_t                   m_event;
    thread_pool_t* const            m_owner;
    void*                           m_param;
    std::auto_ptr<core::thread_t>   m_thread;
    atomic_t                        m_active;
    bool                            m_deleting;

private:
    void execute_loop(void* param) {
        thread_worker_t* const volatile inst = static_cast< thread_worker_t* >(param);

        while (inst->m_active) {
            if (!inst->m_callback.empty()) {
                // Вызвать обработчик
                inst->m_callback(inst->m_param);
                // Очистить параметры
                inst->m_param = NULL;
                inst->m_callback.clear();
            }

            // 1. После выполнения пользовательского задания
            //    поток должен быть возвращен обратно в пул
            inst->m_owner->m_idles.push(inst);

            // 2. Ждать пробуждения потока и параллельно обеспечить
            //    удаление неиспользуемых потоков
            while (true) {
                bool timed = true;
                // Случайный разброс в перидах ожидания для того, чтобы уменьшить
                // вероятность одновременного удаления нескольких потоков
                const core::WaitResult rval = timed ? m_event.wait((60 + rand() % 30) * 1000) : m_event.wait();

                if (rval != core::WR_TIMEOUT)
                    break;
                else
                    timed = inst->m_owner->delete_idle_worker(inst);
            }
        }
    }

public:
    thread_worker_t(thread_pool_t* const owner)
        : m_event(true)
        , m_owner(owner)
        , m_param(NULL)
        , m_active(1)
        , m_deleting(false)
    {
        next = NULL;
        m_event.reset();
    }

    ~thread_worker_t() {
        m_active = 0;
        // -
        if (m_thread.get() != NULL) {
            m_event.signaled();
            // Дождаться завершения системного потока
            m_thread->join();
        }
    }

public:
    void execute(callback_t cb, void* param) {
        // TN:
        m_callback = cb;
        m_param    = param;

        if (m_thread.get() == NULL) {
            m_thread.reset(new core::thread_t(fastdelegate::MakeDelegate(this, &thread_worker_t::execute_loop), this));
        }
        else {
            m_event.signaled();
        }
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
    thread_worker_t* const thread = this->sync_allocate();

    thread->execute(cb, param);
}

//-----------------------------------------------------------------------------
void thread_pool_t::collect_all() {
    // TN: Вызов данного метода может осуществляться из разных потоков
    generics::stack_t< thread_worker_t > queue(m_idles.extract());
    // Удалить все потоки
    while (thread_worker_t* const item = queue.pop())
        delete_worker(item);
}
//-----------------------------------------------------------------------------
bool thread_pool_t::delete_idle_worker(thread_worker_t* const ctx) {
    thread_worker_t* idle = NULL;

    {
        core::MutexLocker lock(m_cs);

        if (!ctx->m_deleting && m_idles.front() != ctx) {
            idle = m_idles.pop();

            if (idle) {
                if (ctx == idle)
                    m_idles.push(idle);
                else
                    idle->m_deleting = true;
            }
        }
    }

    if (idle)
        this->delete_worker(idle);

    return idle != NULL;
}
//-----------------------------------------------------------------------------
void thread_pool_t::delete_worker(thread_worker_t* const item) {
    delete item;
    // -
    atomic_decrement(&m_count);
}
//-----------------------------------------------------------------------------
thread_worker_t* thread_pool_t::sync_allocate() {
    // TN: Вызов данного метода может осуществляться из разных потоков
    if (thread_worker_t* const worker = m_idles.pop())
        return worker;
    else {
        atomic_increment(&m_count);

        return new thread_worker_t(this);
    }
}

} // namespace acto
