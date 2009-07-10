
#ifndef actosystem_platform_h
#define actosystem_platform_h

#if !defined (_WIN64) && !defined (_WIN32)
#   define ACTO_UNIX

#   if defined (__linux__)
#       define ACTO_LINUX
#   else
#       error "Undefined target platform"
#   endif
#else
#   define ACTO_WIN

#   if defined (_WIN64)
#       define ACTO_WIN64
#   elif defined (_WIN32)
#       define ACTO_WIN32
#   else
#       error "Undefined windows platform"
#   endif
#endif


///////////////////////////////////////////////////////////////////////////////
//               НАСТРОЙКА БИБЛИОТЕКИ ПОД ТЕКУЩЙИ КОМПИЛЯТОР                 //
///////////////////////////////////////////////////////////////////////////////


// Используется компилятор Microsoft, либо совместимый
#if defined (_MSC_VER)

#   if !defined _MT
#       error "Multithreaded mode not defined. Use /MD or /MT compiler options."
#   endif

    //  Если текущий режим - отладочный
#   if defined (_DEBUG)
#       if !defined (DEBUG)
#           define ACTO_DEBUG    1
#       endif
#   endif

// Директивы экспорта для сборки DLL
#   ifdef ACTO_EXPORT
#       define ACTO_API         __declspec( dllexport )
    #elif ACTO_IMPORT
#       define ACTO_API         __declspec( dllimport )
#   else
#       define ACTO_API
#   endif

#else
#   define ACTO_API
#endif


///////////////////////////////////////////////////////////////////////////////
//               НАСТРОЙКА БИБЛИОТЕКИ ПОД ЦЕЛЕВУЮ ПЛАТФОРМУ                  //
///////////////////////////////////////////////////////////////////////////////

#if defined ( ACTO_WIN )

#   if !defined (_MSC_VER)
#       error "Unknown windows compiler"
#   endif

#   if !defined ( _WIN32_WINNT )
        // Целевая платформа Windows XP или выше
#       define _WIN32_WINNT     0x0501
#   endif
    // -
#   include <windows.h>

#   define FORCE_INLINE     inline
#   define TLS_VARIABLE     __declspec (thread)

    /* Целые со знаком */
    typedef __int8              i8;
    typedef __int16             i16;
    typedef __int32             i32;
    typedef __int64             i64;

    /* Беззнаковые целые */
    typedef unsigned __int8     ui8;
    typedef unsigned __int16    ui16;
    typedef unsigned __int32    ui32;
    typedef unsigned __int64    ui64;

namespace acto {

namespace core {
    /// Количетсов физически процессоров (ядер) в системе
    inline unsigned int NumberOfProcessors() {
        SYSTEM_INFO     si;
        // -
        ::GetSystemInfo( &si );
        // -
        return si.dwNumberOfProcessors;
    }

    inline void sleep(unsigned int milliseconds) {
        ::Sleep( milliseconds );
    }

    inline void yield() {
        ::SwitchToThread();
    }
}

}

#elif defined (ACTO_LINUX)

#   define FORCE_INLINE     __inline__ __attribute__((always_inline))
#   define TLS_VARIABLE     __thread

#   include <pthread.h>
#   include <stdint.h>
#   include <unistd.h>
#   include <sched.h>

    /* Целые со знаком */
    typedef int8_t              i8;
    typedef int16_t             i16;
    typedef int32_t             i32;
    typedef int64_t             i64;

    /* Беззнаковые целые */
    typedef uint8_t             ui8;
    typedef uint16_t            ui16;
    typedef uint32_t            ui32;
    typedef uint64_t            ui64;

namespace acto {

namespace core {
    /// Количетсов физически процессоров (ядер) в системе
    inline unsigned int NumberOfProcessors() {
        static long n_cpu = ::sysconf(_SC_NPROCESSORS_CONF);

        return n_cpu;
    }

    inline void sleep(unsigned int milliseconds) {
        ::sleep(milliseconds / 1000);
    }

    inline void yield() {
        sched_yield();
    }
}

}

#else
#   define FORCE_INLINE
#   define TLS_VARIABLE
#endif

#endif // actosystem_platform_h
