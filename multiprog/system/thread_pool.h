///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2009                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
///////////////////////////////////////////////////////////////////////////////

#ifndef acot_thread_pool_h_425878588d2e4b6cade43cf0383690b4
#define acot_thread_pool_h_425878588d2e4b6cade43cf0383690b4

#include <system/atomic.h>
#include <system/mutex.h>
#include <system/event.h>

#include <generic/intrlist.h>
#include <generic/queue.h>

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
    static void execute_loop(void* param);

private:
    core::mutex_t   m_cs;
    /// Очередь незадействованных потоков
    idle_queue_t    m_idles;
    /// Очередь заданий, которые еще не обработаны
    task_queue_t    m_tasks;
    /// Текущее количество потоков
    atomic_t        m_count;
};

} // namespace acto

#endif // acot_thread_pool_h_425878588d2e4b6cade43cf0383690b4
