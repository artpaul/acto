
#include "platform.h"
#include "mutex.h"

#if defined (ACTO_WIN)
#   include <windows.h>
#elif defined (ACTO_UNIX)
#   include <pthread.h>
#endif

namespace acto {

namespace core {

#if defined (ACTO_WIN)

/** */
class mutex_t::impl {
    CRITICAL_SECTION    m_section;

public:
    impl() {
        ::InitializeCriticalSection(&m_section);
    }

    ~impl() {
        ::DeleteCriticalSection(&m_section);
    }

public:
    void acquire() {
        ::EnterCriticalSection(&m_section);
    }

    void release() {
        ::LeaveCriticalSection(&m_section);
    }
};

#elif defined (ACTO_UNIX)

/** */
class mutex_t::impl {
    pthread_mutex_t     m_section;

public:
    impl() {
        pthread_mutexattr_t attr;

        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);

        pthread_mutex_init(&m_section, &attr);

        pthread_mutexattr_destroy(&attr);
    }

    ~impl() {
        pthread_mutex_destroy(&m_section);
    }

public:
    void acquire() {
        pthread_mutex_lock(&m_section);
    }

    void release() {
        pthread_mutex_unlock(&m_section);
    }
};

#endif


//-----------------------------------------------------------------------------
mutex_t::mutex_t() 
    : m_pimpl(new impl())
{
}
//-----------------------------------------------------------------------------
mutex_t::~mutex_t() {
    // must be defined for delete m_pimpl
}
//-----------------------------------------------------------------------------
void mutex_t::acquire() {
    m_pimpl->acquire();
}
//-----------------------------------------------------------------------------
void mutex_t::release() {
    m_pimpl->release();
}

} // namespace core

} // namepsace acto
