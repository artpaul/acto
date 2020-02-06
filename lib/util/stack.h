#pragma once

#include "sequence.h"

#include <assert.h>
#include <atomic>

namespace acto {
namespace generics {

/**
 * Используемый алгоритм будет корректно работать, если только
 * единственный поток извлекает объекты из стека и удаляет их.
 */
template <typename T>
class mpsc_stack_t {
public:
    mpsc_stack_t()
        : m_head(nullptr)
    {
    }

    bool empty() const {
        return (m_head.load() == nullptr);
    }

    sequence_t<T> extract() {
        while (true) {
            T* top = m_head.load();

            if (top == nullptr)
                return nullptr;

            if (m_head.compare_exchange_weak(top, nullptr)) {
                return top;
            }
        }
    }

    void push(T* const node) {
        while (true) {
            T* top = m_head.load();

            node->next = top;

            if (m_head.compare_exchange_weak(top, node)) {
                return;
            }
        }
    }

    void push(sequence_t<T> seq) {
        while (T* const item = seq.pop())
            this->push(item);
    }

    T* pop() {
        while (true) {
            T* top = m_head.load();

            if (top == nullptr) {
                return nullptr;
            } else {
                T* next = top->next;

                if (m_head.compare_exchange_weak(top, next)) {
                    top->next = nullptr;
                    return top;
                }
            }
        }
    }

private:
    std::atomic<T*> m_head;
};


/**
 * Простой интрузивный стек без каких-либо блокировок
 */
template <typename T>
class stack_t {
public:
    stack_t()
        : m_head(nullptr)
    {
    }

    stack_t(sequence_t<T> seq)
        : m_head(seq.extract())
    {
    }

    ~stack_t() {
        assert(m_head == nullptr);
    }

    bool empty() const {
        return (m_head == nullptr);
    }

    sequence_t<T> extract() {
        sequence_t<T> top = sequence_t<T>(m_head);
        m_head = nullptr;
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
            result->next = nullptr;
        }
        return result;
    }

private:
    T* m_head;
};

} // namespace generics
} // namespace acto
