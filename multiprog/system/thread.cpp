
#include <generic/delegates.h>

#include "platform.h"
#include "thread.h"

#if defined (ACTO_WIN)
#   include <windows.h>
#elif defined (ACTO_UNIX)
#   include <pthread.h>
#endif


namespace acto {

namespace core {

#if defined (ACTO_WIN)

/** Системный поток в Windows */
class thread_t::impl {
    // Дескриптор потока
    HANDLE      m_handle;
    // Идентификатор потока
    DWORD       m_id;
    // -
    void*       m_param;
    proc_t      m_proc;

private:
    static DWORD WINAPI thread_proc(void* param) {
        if (impl* const p_impl = static_cast< impl* >(param)) {
            if (!p_impl->m_proc.empty()) {
                // Вызвать процедуру потока
                p_impl->m_proc(p_impl->m_param);
            }
        }
        ::ExitThread(0);
    }

public:
    impl(const proc_t& proc, void* param)
        : m_handle(0)
        , m_id    (0)
        , m_param (param)
        , m_proc  (proc)
    {
        m_handle = ::CreateThread(0, 0, &impl::thread_proc, this, 0, &m_id);
    }

    inline ~impl() throw() {
        if (m_handle != 0) {
            // Дождаться завершения потока
            ::WaitForSingleObject(m_handle, 1000);
            // Закрыть дескриптор потока
            ::CloseHandle(m_handle);
        }
    }

public:
    void join() throw() {
        ::WaitForSingleObject(m_handle, INFINITE);
    }
};

#elif defined (ACTO_UNIX)

/** Системный поток в Unix */
class thread_t::impl {
    // Дескриптор потока
    pthread_t   m_handle;
    void*       m_param;
    proc_t      m_proc;

private:
    static void* thread_proc(void* param) {
        if (impl* const p_impl = static_cast<impl*>(param)) {
            if (!p_impl->m_proc.empty()) {
                // Вызвать процедуру потока
                p_impl->m_proc(p_impl->m_param);
            }
        }
        pthread_exit(NULL);
    }

public:
    impl(const proc_t& proc, void* param)
        : m_handle(0)
        , m_param(param)
        , m_proc(proc)
    {
        if (pthread_create(&m_handle, 0, &impl::thread_proc, this) != 0)
            throw "pthread creation error";
    }

    inline ~impl() throw() {
        this->join();
    }

public:
    void join() throw() {
        pthread_join(m_handle, 0);
    }
};

#endif


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
thread_t::thread_t(const proc_t& proc, void* const param) :
    m_param(param),
    m_pimpl(new impl(proc, param))
{
    // -
}
//-----------------------------------------------------------------------------
thread_t::~thread_t() {
    // must be defined for delete m_pimpl
}
//-----------------------------------------------------------------------------
void thread_t::join() {
    m_pimpl->join();
}
//-----------------------------------------------------------------------------
void* thread_t::param() const {
    return m_param;
}

} // namespace core

} // namespace acto
