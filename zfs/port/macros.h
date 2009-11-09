
#ifndef __uril_cl_macros_h__
#define __uril_cl_macros_h__


#define ALIGNING(n)     __attribute__ ((aligned (n)))


#define DECLARE_NONCOPYABLE(T)  private: \
    T(const T& rhs) { } \
    T& operator = (const T& rhs) { return *this; }


#endif // __uril_cl_macros_h__
