#pragma once

#include "atomic.h"
#include "sequence.h"

#include <assert.h>

namespace acto {
namespace generics {

/**
 * Используемый алгоритм будет корректно работать, если только
 * единственный поток извлекает объекты из стека и удаляет их.
 */
template <typename T>
class mpsc_stack_t {
    T* volatile    m_head;

public:
    mpsc_stack_t()
        : m_head(NULL)
    {
    }

    bool empty() const {
        return (m_head == NULL);
    }

    sequence_t<T> extract() {
        while (true) {
            T* const top = m_head;

            if (top == NULL)
                return NULL;
            if (atomic_compare_and_swap((atomic_t*)&m_head, (long)top, 0))
                return top;
        }
    }

    void push(T* const node) {
        while (true) {
            T* const top = m_head;

            node->next = top;
            if (atomic_compare_and_swap((atomic_t*)&m_head, (long)top, (long)node))
                return;
        }
    }

    void push(sequence_t<T> seq) {
        while (T* const item = seq.pop())
            this->push(item);
    }

    T* pop() {
        while (true) {
            T* const top = m_head;

            if (top == NULL)
                return NULL;
            else {
                T* const next = top->next;

                if (atomic_compare_and_swap((atomic_t*)&m_head, (long)top, (long)next)) {
                    top->next = NULL;
                    return top;
                }
            }
        }
    }
};


/**
 * Простой интрузивный стек без каких-либо блокировок
 */
template <typename T>
class stack_t {
    T*     m_head;
public:
    stack_t()
        : m_head(NULL)
    {
    }

    stack_t(sequence_t<T> seq)
        : m_head(seq.extract())
    {
    }

    ~stack_t() {
        assert(m_head == NULL);
    }

    bool empty() const {
        return (m_head == NULL);
    }

    sequence_t<T> extract() {
        sequence_t<T> top = sequence_t<T>(m_head);
        m_head = NULL;
        return top;
    }

    void push(T* const node) {
        node->next = m_head;
        m_head     = node;
    }

    void push(sequence_t<T> seq) {
        while (T* const item = seq.pop())
            this->push(item);
    }

    T* pop() {
        T* const result = m_head;
        // -
        if (result) {
            m_head = result->next;
            result->next = NULL;
        }
        return result;
    }
};

} // namespace generics
} // namespace acto
