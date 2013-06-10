#pragma once

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
        m_head = nullptr;
        return result;
    }

    T* pop() {
        T* const result = m_head;
        // -
        if (result) {
            m_head       = result->next;
            result->next = nullptr;
        }
        // -
        return result;
    }
};

} // namespace generics
} // namespace acto
