#pragma once

#if defined (__linux__)
# define ACTO_LINUX
# define ACTO_UNIX
#elif defined (__APPLE__)
# define ACTO_DARWIN
# define ACTO_UNIX
#elif defined (_WIN64)
# define ACTO_WIN
# define ACTO_WIN64
#else
# error "Unsupported target platform"
#endif


#if defined (_MSC_VER)
# if !defined (_MT)
#   error "Multithreaded mode not defined. Use /MD or /MT compiler options."
# endif


# ifdef ACTO_EXPORT
#   define ACTO_API __declspec( dllexport )
# elif ACTO_IMPORT
#   define ACTO_API __declspec( dllimport )
# else
#   define ACTO_API
# endif
#else
# define ACTO_API
#endif


#if defined (ACTO_WIN)
# include <windows.h>

  /* Basic signed types. */
  typedef __int8              i8;
  typedef __int16             i16;
  typedef __int32             i32;
  typedef __int64             i64;

  /* Basic unsigned types. */
  typedef unsigned __int8     ui8;
  typedef unsigned __int16    ui16;
  typedef unsigned __int32    ui32;
  typedef unsigned __int64    ui64;

  namespace acto {
  namespace core {
    inline unsigned long NumberOfProcessors() {
      SYSTEM_INFO si;
      ::GetSystemInfo(&si);
      return si.dwNumberOfProcessors;
    }

    inline void sleep(unsigned int milliseconds) {
      ::Sleep( milliseconds );
    }

    inline void yield() {
      ::SwitchToThread();
    }
  } // namespace core
  } // namespace acto

#elif defined (ACTO_UNIX)

# include <pthread.h>
# include <stdint.h>
# include <unistd.h>
# include <sched.h>

  /* Basic signed types. */
  typedef int8_t              i8;
  typedef int16_t             i16;
  typedef int32_t             i32;
  typedef int64_t             i64;

  /* Basic unsigned types. */
  typedef uint8_t             ui8;
  typedef uint16_t            ui16;
  typedef uint32_t            ui32;
  typedef uint64_t            ui64;

  namespace acto {
  namespace core {
    inline unsigned long NumberOfProcessors() {
      static const long n_cpu = ::sysconf(_SC_NPROCESSORS_CONF);
      return n_cpu;
    }

    inline void sleep(unsigned int milliseconds) {
      ::sleep(milliseconds / 1000);
    }

    inline void yield() {
      sched_yield();
    }
  } // namespace core
  } // namespace acto

#endif
