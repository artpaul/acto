
#ifndef acto_atomic_h_362EAE7633194578AB306EF6963EDDCB
#define acto_atomic_h_362EAE7633194578AB306EF6963EDDCB

#include "platform.h"

#ifdef _MSC_VER
#   include <windows.h>

    namespace acto {
        typedef volatile long   atomic_t;

        inline long atomic_add(atomic_t* a, long b) { 
            return InterlockedExchangeAdd(a, b) + b;
        }

        inline long atomic_increment(atomic_t* const a) {
            return InterlockedIncrement(a);
        }

        inline long atomic_decrement(atomic_t* const a) {
            return InterlockedDecrement(a);
        }

        inline bool atomic_compare_and_swap(atomic_t* const a, long val, long cmp) {
            return InterlockedCompareExchange(a, val, cmp) == cmp;
        }

        inline long atomic_swap(atomic_t* const a, long b) {
            return InterlockedExchange(a, b);
        }

    } // namespace acto

#elif defined(__GNUC__) 
#   if !defined(__ia64__)
        namespace acto {
            typedef volatile long   atomic_t;

            inline long atomic_add(atomic_t* const a, long b) {
                long tmp = b;

                __asm__ __volatile__(
                    "lock\n\t"
                    "xadd %0, %1\n\t"
                    : "+r" (tmp), "+m" (a)
                    :
                    : "memory");

                return tmp + b;
            }

            inline long atomic_increment(atomic_t* const a) {
                return atomic_add(a, 1);
            }

            inline long atomic_decrement(atomic_t* const a) {
                return atomic_add(a, -1);
            }

            inline bool atomic_compare_and_swap(atomic_t* const a, long val, long cmp) {
                char ret;

                __asm__ __volatile__ (
                    "lock\n\t"
                    "cmpxchg %3,%1\n\t"
                    "sete %0\n\t"
                    : "=q" (ret), "+m" (*(a)), "+a" (cmp)
                    : "r" (val)
                    : "cc", "memory");

                return ret;
            }

            inline long atomic_swap(atomic_t* const a, long b) {
                register long rv = b;

                __asm__ __volatile__ (
                    "lock\n\t"
                    "xchg %0, %1\n\t"
                    : "+m" (a) , "+q" (rv)
                    :
                    : "cc");

                return rv;
            }

        } // namespace acto
#   endif
#endif

#endif // acto_atomic_h_362EAE7633194578AB306EF6963EDDCB
