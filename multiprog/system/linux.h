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
//---------------------------------------------------------------------------//
// File: linux.h                                                             //
//     Обертка над API операционной системы Linux.                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef actosys_linux_h
#define actosys_linux_h

#include <unistd.h>
#include <sched.h>
#include <pthread.h>

#include <typeinfo>

#include "delegates.h"

namespace acto {

namespace core {

class thread_t;
class event_t;


/** */
enum WaitResult {
    // -
    WR_ERROR,
    // -
    WR_SIGNALED,
    // Превышен установленный период ожидания
    WR_TIMEOUT
};


/** */
class thread_t {
public:
    // -
    typedef fastdelegate::FastDelegate< void () >   proc_t;

public:
    thread_t(const proc_t& proc, void* const param = 0)
        : m_handle(0)
        , m_callback(proc)
        , m_param(param)
    {
        if (pthread_create(&m_handle, 0, &thread_t::thread_proc, this) != 0)
            throw "pthread creation error";
    }

    ~thread_t() {
    }

    // Текущий поток для вызываемого метода
    static thread_t*  current() {
        return 0;
        // pthread_self
    }

public:
    void join() {
        pthread_join(m_handle, 0);
    }

private:
    static void* thread_proc(void* param) {
        if (thread_t* const p_thread = static_cast<thread_t*>(param)) {
            if (!p_thread->m_callback.empty()) {
                // Связать экземпляр с потоком
                //instance = p_thread;
                // Вызвать процедуру потока
                p_thread->m_callback();
                // Обнулить связь
                //instance = 0;
            }
        }
        pthread_exit(NULL);
    }

private:
    pthread_t   m_handle;
    proc_t      m_callback;
    void* const m_param;
};

/** */
class event_t {
public:
    void reset() {
        // -
    }

    void signaled() {
        // -
    }

    WaitResult wait() {
        return WR_ERROR;
    }

    WaitResult wait(const unsigned int msec) {
        return WR_ERROR;
    }
};


///////////////////////////////////////////////////////////////////////////////
// Desc: Критическая секция.
class section_t {
public:
    section_t() {
        //::InitializeCriticalSection(&m_section);
    }

    ~section_t() {
        //::DeleteCriticalSection(&m_section);
    }

public:
    // Захватить мютекс
    void acquire() {
        //::EnterCriticalSection(&m_section);
    }
    // Освободить мютекс
    void release() {
        //::LeaveCriticalSection(&m_section);
    }

//private:
//    CRITICAL_SECTION    m_section;
};


class MutexLocker {
public:
    MutexLocker(section_t& mutex) :
        m_mutex( mutex )
    {
        m_mutex.acquire();
    }

    ~MutexLocker() {
        m_mutex.release();
    }

private:
    section_t&  m_mutex;
};


// Desc: Количетсов физически процессоров (ядер) в системе
inline unsigned int NumberOfProcessors() {
    return sysconf(_SC_NPROCESSORS_CONF);
    // For linux:
    // int NUM_PROCS =
}

// Desc:
inline void Sleep(unsigned int milliseconds) {
    //::Sleep( milliseconds );
}

inline void yield() {
    sched_yield();
}


inline void* AtomicCompareExchangePointer(volatile void** dest, void* exchange, void* comperand) {
    //return InterlockedCompareExchangePointer(dest, exchange, comperand);
    return NULL;
}

inline long AtomicCompareExchange(long volatile* dest, long exchange, long comperand) {
    //return InterlockedCompareExchange(dest, exchange, comperand);
    return 0;
}

inline long AtomicDecrement(long volatile* addend) {
    //return InterlockedDecrement(addend);
    return --(*addend);
}

template <typename T>
inline T* AtomicExchange(T* volatile* target, T* const value) {
    return static_cast< T* >(InterlockedExchangePointer((volatile void**)target, value));
}

inline long AtomicIncrement(long volatile* addend) {
    //return InterlockedIncrement(addend);
    return ++(*addend);
}

} // namespace core

} // namespace acto

#endif // actosys_linux_h
