
#ifndef acto_generic_memory_h_36143943703c4264bd0fdc0812683f37
#define acto_generic_memory_h_36143943703c4264bd0fdc0812683f37

#include <cstddef>

namespace acto {

namespace generics {

#define DECLARE_NONCOPYABLE(T)  private: \
    T(const T& rhs) { } \
    T& operator = (const T& rhs) { return *this; }


/**
 * Обертка для управления указателями на массив данных
 */
template <typename T>
class array_ptr {
    T*  m_ptr;

    DECLARE_NONCOPYABLE(array_ptr)

private:
    inline void clean() {
        delete [] m_ptr;
    }

public:
    array_ptr() : m_ptr(NULL) {
    }

    explicit array_ptr(T* const ptr) : m_ptr(ptr) {
    }

    ~array_ptr() {
        if (m_ptr)
            this->clean();
    }

public:
    inline T* get() const {
        return m_ptr;
    }
    /// Установить новое значение указателя
    void reset(T* const ptr) {
        if (m_ptr)
            this->clean();
        m_ptr = ptr;
    }
    /// Обменять значения указателей
    void swap(array_ptr& rhs) {
        T* const tmp = m_ptr;
        m_ptr     = rhs.m_ptr;
        rhs.m_ptr = tmp;
    }

    inline bool valid() const {
        return m_ptr != 0;
    }

    inline T& operator [] (const size_t index) {
        return m_ptr[index];
    }

    inline const T& operator [] (const size_t index) const {
        return m_ptr[index];
    }

    inline T& operator * () const {
        return *m_ptr;
    }

    inline T* operator -> () const {
        return m_ptr;
    }
};


#undef DECLARE_NONCOPYABLE

} // namespace generics

} // namespace acto


template <typename T>
inline ptrdiff_t operator - (const T* lhs, const acto::generics::array_ptr<T>& rhs) {
    return lhs - rhs.get();
}

template <typename T>
inline ptrdiff_t operator - (const acto::generics::array_ptr<T>& lhs, const T* rhs) {
    return lhs.get() - rhs;
}

template <typename T>
inline T* operator + (const acto::generics::array_ptr<T>& lhs, const size_t rhs) {
    return lhs.get() + rhs;
}

#endif // acto_generic_memory_h_36143943703c4264bd0fdc0812683f37
