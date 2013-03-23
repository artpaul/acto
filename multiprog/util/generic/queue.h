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
        : m_tail(NULL)
    {
        // -
    }

    sequence_t<T> extract() {
        guard g(m_cs);

        if (m_tail) {
           T* const head = m_tail->next;

           m_tail->next = NULL;
           m_tail       = NULL;
           return sequence_t<T>(head);
        }
        return NULL;
    }

    T* front() const {
        guard g(m_cs);

        return m_tail ? m_tail->next : NULL;
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
                m_tail = NULL;
            else
                m_tail->next = m_tail->next->next;

            result->next = NULL;
            return result;
        }
        return NULL;
    }

    bool empty() const {
        guard g(m_cs);
        return (m_tail == NULL);
    }

private:
    T*                  m_tail;
    mutable std::mutex  m_cs;
};

} // namepsace generics
} // namespace acto
