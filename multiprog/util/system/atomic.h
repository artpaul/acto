#ifndef acto_atomic_h_362EAE7633194578AB306EF6963EDDCB
#define acto_atomic_h_362EAE7633194578AB306EF6963EDDCB

#include "platform.h"

#ifdef _MSC_VER
#   include <windows.h>

    namespace acto {
        typedef volatile long   atomic_t;

        FORCE_INLINE long atomic_add(atomic_t* a, long b) throw () {
            return InterlockedExchangeAdd(a, b) + b;
        }

        FORCE_INLINE long atomic_increment(atomic_t* const a) throw () {
            return InterlockedIncrement(a);
        }

        FORCE_INLINE long atomic_decrement(atomic_t* const a) throw () {
            return InterlockedDecrement(a);
        }

        FORCE_INLINE bool atomic_compare_and_swap(atomic_t* const a, long compare, long val) throw () {
            return InterlockedCompareExchange(a, val, compare) == compare;
        }

        FORCE_INLINE long atomic_swap(atomic_t* const a, long b) throw () {
            return InterlockedExchange(a, b);
        }

    } // namespace acto

#elif defined(__GNUC__)
#   if defined (__x86_64__) || defined(__ia64__)

        namespace acto {
            typedef volatile long   atomic_t;

            static FORCE_INLINE long atomic_add(atomic_t* const a, long b) throw () {
                return __sync_add_and_fetch(a, b);
            }

            static FORCE_INLINE long atomic_increment(atomic_t* const a)  throw () {
                return __sync_add_and_fetch(a, 1);
            }

            static FORCE_INLINE long atomic_decrement(atomic_t* const a)  throw () {
                return __sync_sub_and_fetch(a, 1);
            }

            static FORCE_INLINE bool atomic_compare_and_swap(atomic_t* const a, long compare, long val)  throw () {
                return __sync_bool_compare_and_swap(a, compare, val);
            }

            static FORCE_INLINE long atomic_swap(atomic_t* const a, long b)  throw () {
                return __sync_lock_test_and_set(a, b);
            }
        } // namespace acto
#   else
#       define ATOMICOPS_WORD_SUFFIX "l"

        namespace acto {
            typedef volatile long   atomic_t;

             static FORCE_INLINE long atomic_add(atomic_t* const a, long b) throw () {
                  long temp = b;
                  __asm__ __volatile__("lock; xadd" ATOMICOPS_WORD_SUFFIX " %0,%1"
                                       : "+r" (b), "+m" (*a)
                                       : : "memory");
                  // temp now contains the previous value of *a
                  return temp + b;
            }

            static FORCE_INLINE long atomic_increment(atomic_t* const a) throw () {
                return atomic_add(a, 1);
            }

            static FORCE_INLINE long atomic_decrement(atomic_t* const a) throw () {
                return atomic_add(a, -1);
            }

            static FORCE_INLINE bool atomic_compare_and_swap(atomic_t* const a, long compare, long val) throw () {
                char ret;

                __asm__ __volatile__ (
                    "lock\n\t"
                    "cmpxchg %3,%1\n\t"
                    "sete %0\n\t"
                    : "=q" (ret), "+m" (*(a)), "+a" (compare)
                    : "r" (val)
                    : "cc", "memory");

                return ret;
            }

            static FORCE_INLINE long atomic_swap(atomic_t* const a, long b) throw () {
                __asm__ __volatile__("xchg" ATOMICOPS_WORD_SUFFIX " %1,%0"  // The lock prefix
                                   : "=r" (b)                               // is implicit for
                                   : "m" (*a), "0" (b)                      // xchg.
                                   : "memory");
                return b;  // Now it's the previous value.
            }

        } // namespace acto
#       undef ATOMICOPS_WORD_SUFFIX
#   endif
#endif

#endif // acto_atomic_h_362EAE7633194578AB306EF6963EDDCB
