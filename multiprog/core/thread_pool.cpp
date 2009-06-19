
#include "thread_pool.h"

namespace acto {

///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
thread_worker_t::thread_worker_t(thread_pool_t* const owner)
    : m_owner(owner)
    , m_param(NULL)
    , m_active(true)
{
    m_event.reset();
}
//-----------------------------------------------------------------------------
thread_worker_t::~thread_worker_t() {
    m_active = false;
    // -
    if (m_thread.get() != NULL) {
        m_event.signaled();
	    // Дождаться завершения системного потока
        m_thread->join();
    }
}
//-----------------------------------------------------------------------------
void thread_worker_t::start(callback_t cb, void* param) {
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
//-----------------------------------------------------------------------------
void thread_worker_t::execute_loop(void* param) {
    thread_worker_t* const inst = static_cast<thread_worker_t*>(param);

    while (inst->m_active) {
        inst->m_callback(inst->m_param);
        // После выполнения пользовательского задания
        // поток должен быть возвращен обратно в пул
        inst->m_owner->m_idles.push(inst);
        // -
        m_event.wait();
        m_event.reset();
    }
}

} // namespace acto
