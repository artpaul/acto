
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
    callback_t  m_proc;

private:
    static DWORD WINAPI thread_proc(void* param) {
        if (impl* const pthis = static_cast< impl* >(param)) {
            if (pthis->m_proc != NULL) {
                // Вызвать процедуру потока
                pthis->m_proc(pthis->m_param);
            }
        }
        ::ExitThread(0);
    }

public:
    impl(const callback_t proc, void* param)
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

    void* param() const {
        return m_param;
    }
};

#elif defined (ACTO_UNIX)

/** Системный поток в Unix */
class thread_t::impl {
    // Дескриптор потока
    pthread_t   m_handle;
    void*       m_param;
    callback_t  m_proc;

private:
    static void* thread_proc(void* param) {
        if (impl* const pthis = static_cast< impl* >(param)) {
            if (pthis->m_proc != NULL) {
                // Вызвать процедуру потока
                pthis->m_proc(pthis->m_param);
            }
        }
        pthread_exit(NULL);
    }

public:
    impl(const callback_t proc, void* param)
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

    void* param() const {
        return m_param;
    }
};

#endif


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
thread_t::thread_t(const callback_t proc, void* const param)
    : m_pimpl(new impl(proc, param))
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
    return m_pimpl->param();
}

} // namespace core

} // namespace acto
