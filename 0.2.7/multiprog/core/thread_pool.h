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

#include <generic/delegates.h>
#include <system/atomic.h>
#include <system/event.h>

#include <core/struct.h>

namespace acto {

class thread_worker_t;


/**
 * Пул потоков - разделяемый ресурс между несколькими
 *               подсистемами библиотеки
 */
class thread_pool_t {
    friend class thread_worker_t;

    typedef fastdelegate::FastDelegate< void (void*) >  callback_t;
    typedef core::structs::queue_t< thread_worker_t >   idle_queue_t;

public:
    thread_pool_t();
    ~thread_pool_t();

    static thread_pool_t* instance();

public:
    /// Удалить один простаивающий поток
    void collect_one();
    /// Удалить все простаивающие потоки
    void collect_all();
    ///
    void queue_task(callback_t cb, void* param);

private:
    void delete_worker(thread_worker_t* const item);
    // -
    thread_worker_t* sync_allocate();

private:
    idle_queue_t    m_idles;
    atomic_t        m_count;
};

} // namespace acto

#endif // acot_thread_pool_h_425878588d2e4b6cade43cf0383690b4