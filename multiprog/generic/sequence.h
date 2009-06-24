
#ifndef sequence_h_B7DF74B10AB24b058D94FBF47080CEFE
#define sequence_h_B7DF74B10AB24b058D94FBF47080CEFE

namespace acto { 
    
namespace generics {

/** */
template <typename T>
class sequence_t {
    T*  m_head;

public:
    sequence_t(T* const item) 
        : m_head(item) 
    { 
    }

    T* extract() {
        T* const result = m_head;
        m_head = NULL;
        return result;
    }

    T* pop() {
        T* const result = m_head;
        // -
        if (result) {
            m_head       = result->next;
            result->next = NULL;
        }
        // -
        return result;
    }
};

} // namespace generics

} // namespace acto

#endif // sequence_h_B7DF74B10AB24b058D94FBF47080CEFE
