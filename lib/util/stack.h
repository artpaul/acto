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
  bool empty() const {
    return (m_head.load() == nullptr);
  }

  sequence_t<T> extract() {
    do {
      T* top = m_head.load();

      if (top == nullptr) {
        return nullptr;
      }
      if (m_head.compare_exchange_weak(top, nullptr)) {
        return top;
      }
    } while (true);
  }

  void push(T* const node) {
    do {
      T* top = m_head.load();
      node->next = top;
      if (m_head.compare_exchange_weak(top, node)) {
        return;
      }
    } while (true);
  }

  void push(sequence_t<T>&& seq) {
    while (T* const item = seq.pop()) {
      this->push(item);
    }
  }

  T* pop() {
    do {
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
    } while (true);
  }

private:
  std::atomic<T*> m_head{nullptr};
};


/**
 * Простой интрузивный стек без каких-либо блокировок
 */
template <typename T>
class stack_t {
public:
  stack_t() noexcept = default;

  stack_t(sequence_t<T>&& seq) noexcept
    : m_head(seq.extract())
  {
  }

  ~stack_t() {
    assert(m_head == nullptr);
  }

  bool empty() const noexcept {
    return !m_head;
  }

  sequence_t<T> extract() noexcept {
    sequence_t<T> top = sequence_t<T>(m_head);
    m_head = nullptr;
    return top;
  }

  void push(T* const node) noexcept {
    node->next = m_head;
    m_head = node;
  }

  void push(sequence_t<T>&& seq) noexcept {
    while (T* const item = seq.pop()) {
      this->push(item);
    }
  }

  T* pop() noexcept {
    T* const result = m_head;

    if (result) {
      m_head = result->next;
      result->next = nullptr;
    }
    return result;
  }

private:
  T* m_head{nullptr};
};

} // namespace generics
} // namespace acto
