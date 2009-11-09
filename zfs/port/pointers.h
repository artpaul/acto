
#ifndef __util_cl_pointers_h__
#define __util_cl_pointers_h__

#include <stddef.h>

#include "macros.h"


namespace cl {


namespace detail {

template <typename T>
class ptrcleaner {
public:
    static inline void clean(T*& ptr) {
        delete ptr, ptr = 0;
    }
};

template <typename T>
class arraycleaner {
public:
    static inline void clean(T*& ptr) {
        delete [] ptr, ptr = 0;
    }
};

}; // namespace detail


/**
 *
 */
template <typename T, template <class> class Cleaner = detail::ptrcleaner>
class auto_ptr {
    T*  m_ptr;

    DECLARE_NONCOPYABLE(auto_ptr)

public:
    auto_ptr() : m_ptr(0) {
    }

    explicit auto_ptr(T* const ptr) : m_ptr(ptr) {
    }

    ~auto_ptr() {
        if (m_ptr)
            Cleaner<T>::clean(m_ptr);
    }

public:
    inline T* get() const {
        return m_ptr;
    }
    /// Установить новое значение указателя
    void reset(T* const ptr) {
        if (m_ptr)
            Cleaner<T>::clean(m_ptr);
        m_ptr = ptr;
    }
    /// Обменять значения указателей
    void swap(auto_ptr& rhs) {
        T* const tmp = m_ptr;
        m_ptr     = rhs.m_ptr;
        rhs.m_ptr = tmp;
    }

    inline bool valid() const {
        return m_ptr != 0;
    }

    inline T& operator * () const {
        return *m_ptr;
    }

    inline T* operator -> () const {
        return m_ptr;
    }
};


/**
 * Обертка для управления указателями на массив данных
 */
template <typename T>
class array_ptr : public auto_ptr<T, detail::arraycleaner> {
    typedef auto_ptr<T, detail::arraycleaner>   base;

    DECLARE_NONCOPYABLE(array_ptr)

public:
    array_ptr() : base() {
    }

    explicit array_ptr(T* const ptr) : base(ptr) {
    }
};


template <typename T>
inline ptrdiff_t operator - (const T* lhs, const array_ptr<T>& rhs) {
    return lhs - rhs.get();
}

template <typename T>
inline ptrdiff_t operator - (const array_ptr<T>& lhs, const T* rhs) {
    return lhs.get() - rhs;
}

template <typename T>
inline T* operator + (const array_ptr<T>& lhs, const size_t rhs) {
    return lhs.get() + rhs;
}

}; // namespace cl

#endif // __util_cl_pointers_h__
