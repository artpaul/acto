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
// File: sys_mswin.h                                                         //
//     Обертка над API операционной системы MS Windows.                      //
///////////////////////////////////////////////////////////////////////////////

#if !defined __multiprog__sys_mswin_h__
#define __multiprog__sys_mswin_h__

#include <windows.h>

#include <config.h>

#include "delegates.h"

namespace acto {

namespace system {

///////////////////////////////////////////////////////////////////////////////
// Desc: Системный поток.
class thread_t {
public:
    // Тип системного идентификатора для потокаы
    typedef DWORD   identifier_type;

    // -
    typedef fastdelegate::FastDelegate< void () >   proc_t;

public:
    // Дескриптор потока
    HANDLE      m_handle;
    // Идентификатор потока
    DWORD       m_id;
    // -
    void* const m_param;
    // -
    proc_t      m_proc;

public:
     thread_t(const proc_t& proc, void* const param = 0);
    ~thread_t();

public:
    void join() {
        ::WaitForSingleObject( m_handle, INFINITE );
    }

    void join(const unsigned int msec) {
        ::WaitForSingleObject( m_handle, msec );
    }

    // Пользовательские данные, связанные с текущим потоком
    void* param() const;

public:
    // Текущий поток для вызываемого метода
    static thread_t*  current();

private:
    static DWORD WINAPI thread_proc(LPVOID lpParameter);

    TLS_VARIABLE static thread_t*  instance;
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//                                ПРИМИТИВЫ СИНХРОНИЗАЦИИ                                        //
///////////////////////////////////////////////////////////////////////////////////////////////////

enum WaitResult {
    // -
    wrError,
    // -
    wrSignaled,
    // Превышен установленный период ожидания
    wrTimeout
};

///////////////////////////////////////////////////////////////////////////////
// Desc: Событие.
class event_t {
public:
    event_t() {
        m_handle = ::CreateEvent(0, TRUE, TRUE, 0);
    }

    ~event_t() {
        ::CloseHandle( m_handle );
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
            return wrSignaled;
        case WAIT_TIMEOUT :
            return wrTimeout;
        }
        return wrError;
    }

    WaitResult wait(const unsigned int msec) {
        switch (::WaitForSingleObject(m_handle, msec)) {
        case WAIT_OBJECT_0 :
            return wrSignaled;
        case WAIT_TIMEOUT :
            return wrTimeout;
        }
        return wrError;
    }

private:
    HANDLE  m_handle;
};


///////////////////////////////////////////////////////////////////////////////
// Desc: Критическая секция.
class section_t {
public:
    section_t() {
        ::InitializeCriticalSection(&m_section);
    }

    ~section_t() {
        ::DeleteCriticalSection(&m_section);
    }

public:
    // Захватить мютекс
    void acquire() {
        ::EnterCriticalSection(&m_section);
    }
    // Освободить мютекс
    void release() {
        ::LeaveCriticalSection(&m_section);
    }

private:
    CRITICAL_SECTION    m_section;
};


///////////////////////////////////////////////////////////////////////////////
// Desc:
class semaphore_t {
public:
    semaphore_t() :
        m_handle( 0 )
    {
        m_handle = ::CreateSemaphore(0, 0, MAXLONG, 0);
    }

    ~semaphore_t() {
        ::CloseHandle( m_handle );
    }

public:
    void release(int count) {
        ::ReleaseSemaphore(m_handle, count, 0);
    }

    void wait() {
        ::WaitForSingleObject(m_handle, INFINITE);
    }

private:
    // Дескриптор семафора
    HANDLE  m_handle;
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




// Инициализация пространства имен
void initialize();

// Завершение использования
void finalize();


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// Desc: Количетсов физически процессоров (ядер) в системе
inline unsigned int NumberOfProcessors() {
    SYSTEM_INFO     si;
    // -
    ::GetSystemInfo( &si );
    // -
    return si.dwNumberOfProcessors;
    // For linux:
    // int NUM_PROCS = sysconf(_SC_NPROCESSORS_CONF);
}

// Desc:
inline void Sleep(unsigned int milliseconds) {
    ::Sleep( milliseconds );
}

inline void yield() {
    ::SwitchToThread();
}


inline void* AtomicCompareExchangePointer(volatile PVOID* dest, void* exchange, void* comperand) {
    return InterlockedCompareExchangePointer(dest, exchange, comperand);
}

inline long AtomicCompareExchange(long volatile* dest, long exchange, long comperand) {
    return InterlockedCompareExchange(dest, exchange, comperand);
}

inline long AtomicDecrement(long volatile* addend) {
    return InterlockedDecrement(addend);
}

template <typename T>
inline T* AtomicExchange(T* volatile* target, T* const value) {
    return static_cast< T* >(InterlockedExchangePointer((volatile PVOID*)target, value));
}

inline long AtomicIncrement(long volatile* addend) {
    return InterlockedIncrement(addend);
}


}; // namespace system

}; // namespace acto

#endif // __multiprog__sys_mswin_h__
