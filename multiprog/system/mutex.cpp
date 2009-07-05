
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
    impl() throw () {
        ::InitializeCriticalSection(&m_section);
    }

    ~impl() throw () {
        ::DeleteCriticalSection(&m_section);
    }

public:
    void acquire() throw () {
        ::EnterCriticalSection(&m_section);
    }

    void release() throw () {
        ::LeaveCriticalSection(&m_section);
    }
};

#elif defined (ACTO_UNIX)

/** */
class mutex_t::impl {
    pthread_mutex_t     m_section;

public:
    impl() throw () {
        pthread_mutexattr_t attr;

        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);

        pthread_mutex_init(&m_section, &attr);

        pthread_mutexattr_destroy(&attr);
    }

    ~impl() throw () {
        pthread_mutex_destroy(&m_section);
    }

public:
    void acquire() throw () {
        pthread_mutex_lock(&m_section);
    }

    void release() throw () {
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
