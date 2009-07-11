
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

    static TLS_VARIABLE impl*  instance;

private:
    static DWORD WINAPI thread_proc(void* param) {
        if (impl* const p_impl = static_cast< impl* >(param)) {
            if (!p_impl->m_proc.empty()) {
                // Связать экземпляр с потоком
                instance = p_impl;
                // Вызвать процедуру потока
                p_impl->m_proc(p_impl->m_param);
                // Обнулить связь
                instance = NULL;
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

    // Текущий поток для вызываемого метода
    static bool is_core_thread() {
        return instance != NULL;
    }

public:
    void join() {
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

    static TLS_VARIABLE impl*  instance;

private:
    static void* thread_proc(void* param) {
        if (impl* const p_impl = static_cast<impl*>(param)) {
            if (!p_impl->m_proc.empty()) {
                // Связать экземпляр с потоком
                instance = p_impl;
                // Вызвать процедуру потока
                p_impl->m_proc(p_impl->m_param);
                // Обнулить связь
                instance = NULL;
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
        // -
    }

    // Текущий поток для вызываемого метода
    static bool is_core_thread() {
        return instance != NULL;
    }

public:
    void join() {
        pthread_join(m_handle, 0);
    }
};

#endif

/// -
TLS_VARIABLE thread_t::impl*  thread_t::impl::instance = NULL;


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
bool thread_t::is_core_thread() {
    return impl::is_core_thread();
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