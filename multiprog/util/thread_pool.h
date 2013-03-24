#pragma once

#include "atomic.h"
#include "event.h"
#include "intrlist.h"
#include "queue.h"

#include <mutex>

namespace acto {

/**
 * Пул потоков - разделяемый ресурс между несколькими
 *               подсистемами библиотеки
 */
class thread_pool_t {
    struct thread_data_t;
    struct task_t;

    typedef void (*callback_t)(void*);
    typedef generics::queue_t< thread_data_t > idle_queue_t;
    typedef generics::queue_t< task_t >        task_queue_t;

    ///
    struct task_t : public core::intrusive_t< task_t > {
        callback_t  callback;
        void*       param;

    public:
        task_t() : callback(NULL), param(NULL) { }
        task_t(callback_t cb, void* p) : callback(cb), param(p) { }
    };

public:
    thread_pool_t();
    ~thread_pool_t();

    static thread_pool_t* instance();

public:
    ///
    void queue_task(callback_t cb, void* param);

private:
    /// Удалить все простаивающие потоки
    void collect_all();
    /// Удалить неиспользуемый поток
    bool delete_idle_worker(thread_data_t* const ctx);
    ///
    void delete_worker(thread_data_t* const item);

    /// -
    static void execute_loop(thread_data_t* pthis);

private:
    std::mutex      m_cs;
    ///
    core::event_t   m_clean;
    /// Очередь незадействованных потоков
    idle_queue_t    m_idles;
    /// Очередь заданий, которые еще не обработаны
    task_queue_t    m_tasks;
    /// Текущее количество потоков
    atomic_t        m_count;
};

} // namespace acto
