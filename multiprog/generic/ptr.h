
#ifndef actogeneric_ptr_h
#define actogeneric_ptr_h

namespace acto {

/** */
template <typename T>
class shared_ptr {
    T*  m_ptr;

public:
    explicit shared_ptr(T* const) : m_ptr(ptr) {
    }

    ~shared_ptr() {
    }

public:
    T* operator -> () {
        return m_ptr;
    }
};

} // namespace acto

#endif // actogeneric_ptr_h
