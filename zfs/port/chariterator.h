
#ifndef __util_cl_chariterator_h__
#define __util_cl_chariterator_h__

#include <stddef.h>


namespace cl {

/** */
template <typename T>
class chariteratorT {
    template <typename P>
    friend inline ptrdiff_t operator - (P* lhs, const chariteratorT<P>& rhs);

protected:
    T*  m_bound;
    T*  m_p;

    inline bool isnotbound() const {
        return (m_p != 0) && *m_p && (m_bound ? m_p != m_bound : true);
    }
    
    inline void next() {
        if (isnotbound()) 
            ++m_p;
    }

public:
    chariteratorT() : m_bound(0), m_p(0) {
    }

    chariteratorT(const chariteratorT& it) : m_bound(it.m_bound), m_p(it.m_p) {
    }

    chariteratorT(T* str, const size_t len) : m_bound(str + len), m_p(str) {
    }
    
    chariteratorT(T* str, T* bound) : m_bound(bound), m_p(str) {
    }

    chariteratorT(const chariteratorT& it, const size_t len) : m_bound(it.m_p + len), m_p(it.m_p) {
        if (it.m_bound != 0 && (it.m_p + len > it.m_bound))
            throw errors::outofbound();
    }

    explicit chariteratorT(T* str) : m_bound(0), m_p(str) {
    }

public:
    /// Размер обрабатываемой последовательности
    /// от текущего значения до ограничителя
    inline size_t size() const {
        if (m_p == 0)
            return 0;
        return (m_bound != 0) ? m_bound - m_p : strlen(m_p);
    }

    inline bool valid() const {
        return m_p != 0 && isnotbound();
    }

    char operator * () const {
        return *m_p;
    }

    const char* operator ~ () const {
        return m_p;
    }
        
    inline bool operator > (const chariteratorT& rhs) const {
        return this->m_p > rhs.m_p;
    }

    chariteratorT& operator ++ () {
        this->next();
        return *this;
    }

    chariteratorT operator ++ (int) {
        chariteratorT tmp(*this);
        this->next();
        return tmp;
    }

    inline chariteratorT operator + (const size_t rhs) const {
        if (m_bound != 0 && (m_bound - m_p < (ptrdiff_t)rhs))
            throw errors::outofbound();
        return chariteratorT(this->m_p + rhs, this->m_bound);
    }

    inline ptrdiff_t operator - (const chariteratorT& rhs) const {
        return this->m_p - rhs.m_p;
    }

    inline chariteratorT& operator += (const size_t rhs) {
        if (m_bound != 0 && (m_bound - m_p < (ptrdiff_t)rhs))
            throw errors::outofbound();
        m_p += rhs;
        return *this;
    } 
};

template <typename T>
inline ptrdiff_t operator - (T* lhs, const chariteratorT<T>& rhs) {
    return lhs - rhs.m_p;
} 


/** */
typedef chariteratorT<char>         char_iterator;
/** */
typedef chariteratorT<const char>   const_char_iterator;


}; // namespace cl

#endif // __util_cl_chariterator_h__
