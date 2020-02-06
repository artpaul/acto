#pragma once

#include "sequence.h"

#include <assert.h>
#include <mutex>

namespace acto {
namespace generics {

/**
 */
template <typename T>
class queue_t {
    typedef std::lock_guard<std::mutex> guard;

public:
    queue_t()
        : m_tail(nullptr)
    {
    }

    sequence_t<T> extract() {
        guard g(m_cs);

        if (m_tail) {
           T* const head = m_tail->next;

           m_tail->next = nullptr;
           m_tail       = nullptr;
           return sequence_t<T>(head);
        }
        return nullptr;
    }

    T* front() const {
        guard g(m_cs);

        return m_tail ? m_tail->next : nullptr;
    }

    void push(T* const node) {
        guard g(m_cs);

        if (m_tail) {
            node->next   = m_tail->next;
            m_tail->next = node;
        } else {
            node->next = node;
        }
        m_tail = node;
    }

    T* pop() {
        guard g(m_cs);

        if (m_tail) {
            T* const result = m_tail->next;

            if (result == m_tail)
                m_tail = nullptr;
            else
                m_tail->next = m_tail->next->next;

            result->next = nullptr;
            return result;
        }
        return nullptr;
    }

    bool empty() const {
        guard g(m_cs);
        return (m_tail == nullptr);
    }

private:
    T*                  m_tail;
    mutable std::mutex  m_cs;
};

} // namepsace generics
} // namespace acto
