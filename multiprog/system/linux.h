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

} // namespace core

} // namespace acto

#endif // actosys_linux_h
