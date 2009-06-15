
#include "platform.h"
#include "event.h"

#if defined (ACTO_WIN)
#   include <windows.h>
#elif defined (ACTO_UNIX)
#   include <pthread.h>
#   include <time.h>
#   include <errno.h>
#endif


namespace acto {

namespace core {

#if defined (ACTO_WIN)

/** */
class event_t::impl {
    HANDLE  m_handle;

public:
    impl() {
        m_handle = ::CreateEvent(0, TRUE, TRUE, 0);
    }

    ~impl() {
        ::CloseHandle(m_handle);
    }

public:
    void reset() {
        ::ResetEvent(m_handle);
    }

    void signaled() {
        ::SetEvent(m_handle);
    }

    WaitResult wait() {
        switch (::WaitForSingleObject(m_handle, INFINITE)) {
        case WAIT_OBJECT_0 :
            return WR_SIGNALED;
        case WAIT_TIMEOUT :
            return WR_TIMEOUT;
        }
        return WR_ERROR;
    }

    WaitResult wait(const unsigned int msec) {
        switch (::WaitForSingleObject(m_handle, msec)) {
        case WAIT_OBJECT_0 :
            return WR_SIGNALED;
        case WAIT_TIMEOUT :
            return WR_TIMEOUT;
        }
        return WR_ERROR;
    }
};

#elif defined (ACTO_UNIX)

/** */
class event_t::impl {
    pthread_mutex_t m_mutex;
    pthread_cond_t  m_cond;
    bool            m_triggered;

public:
    impl() {
        pthread_mutex_init(&m_mutex, 0);
        pthread_cond_init(&m_cond, 0);
        m_triggered = false;
    }

    ~impl() {
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
    }

public:
    void reset() {
        pthread_mutex_lock(&m_mutex);
        m_triggered = false;
        pthread_mutex_unlock(&m_mutex);
    }

    void signaled() {
        pthread_mutex_lock(&m_mutex);
        m_triggered = true;
        pthread_cond_signal(&m_cond);
        pthread_mutex_unlock(&m_mutex);
    }

    WaitResult wait() {
        WaitResult  result = WR_SIGNALED;

        pthread_mutex_lock(&m_mutex);
        while (!m_triggered && result != WR_ERROR) {
            const int rval = pthread_cond_wait(&m_cond, &m_mutex);

            if (rval != 0)
                result = WR_ERROR;
        }
        pthread_mutex_unlock(&m_mutex);

        return result;
    }

    WaitResult wait(const unsigned int msec) {
        WaitResult  result = WR_SIGNALED;
        timespec    time;

        clock_gettime(CLOCK_MONOTONIC, &time);

        time.tv_sec += 1000 * msec;

        pthread_mutex_lock(&m_mutex);
        while (!m_triggered && (result != WR_TIMEOUT && result != WR_ERROR)) {
            const int rval = pthread_cond_timedwait(&m_cond, &m_mutex, &time);

            if (rval == ETIMEDOUT)
                result = WR_TIMEOUT;
            else if (rval != 0)
                result = WR_ERROR;
        }
        pthread_mutex_unlock(&m_mutex);

        return result;
    }
};

#endif


//-----------------------------------------------------------------------------
event_t::event_t()
    : m_pimpl(new impl())
{
}
//-----------------------------------------------------------------------------
event_t::~event_t() {
    // must be defined for delete m_pimpl
}
//-----------------------------------------------------------------------------
void event_t::reset() {
    m_pimpl->reset();
}
//-----------------------------------------------------------------------------
void event_t::signaled() {
    m_pimpl->signaled();
}
//-----------------------------------------------------------------------------
WaitResult event_t::wait() {
    return m_pimpl->wait();
}
//-----------------------------------------------------------------------------
WaitResult event_t::wait(const unsigned int msec) {
    return m_pimpl->wait(msec);
}

} // namespace core

} // namespace acto