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
    pthread_t   m_handle;
    proc_t      m_callback;
    void* const m_param;

private:
    static void* thread_proc(void* param) {
        if (thread_t* const p_thread = static_cast<thread_t*>(param)) {
            if (!p_thread->m_proc.empty()) {
                // Связать экземпляр с потоком
                //instance = p_thread;
                // Вызвать процедуру потока
                p_thread->m_proc();
                // Обнулить связь
                //instance = 0;
            }
        }
        pthread_exit(NULL);
    }

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
