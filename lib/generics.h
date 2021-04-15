#pragma once

#include <atomic>
#include <cassert>
#include <mutex>

namespace acto {
namespace generics {

/**
 * Интрузивный элемент списка
 */
template <typename T>
struct intrusive_t {
  T* next{nullptr};
};


template <typename T>
class intrusive_queue_t {
public:
  /// Отцепить все элементы от очереди сохраняя порядок элементов
  T* detatch() noexcept {
    T* const result = m_head;

    m_head = m_tail = nullptr;
    m_size = 0;

    return result;
  }
  ///
  bool empty() const noexcept {
    return m_head == nullptr;
  }
  /// Указатель на первый элемент
  T* front() const {
    return m_head;
  }
  ///
  T* pop() {
    T* const result = this->remove(m_head, nullptr);
    if (result) {
      result->next = nullptr;
    }
    return result;
  }
  ///
  void push(T* item) {
    item->next = nullptr;

    if (m_head == nullptr) {
      m_head = m_tail = item;
    } else {
      m_tail->next = item;
      m_tail = item;
    }
    ++m_size;
  }

  /// Удаляет указанный элемен из очереди
  ///
  /// \return Указатель на следующий за удаляемым элемент
  T* remove(T* const item, T* const prev) {
    assert(!prev || prev->next == item);

    if (m_head != nullptr) {
      assert(m_size != 0);

      --m_size;

      if (item == m_head) {
        assert(prev == nullptr);

        if (m_head == m_tail) {
          m_head = m_tail = nullptr;
          return nullptr;
        } else {
          m_head = item->next;
          return item->next;
        }
      } else if (item == m_tail) {
        assert(m_head != m_tail);

        m_tail = prev;
        m_tail->next = nullptr;
        return nullptr;
      } else {
        prev->next = item->next;
        return item->next;
      }
    }
    return nullptr;
  }

  size_t size() const noexcept {
    return m_size;
  }

private:
  T* m_head{nullptr};
  T* m_tail{nullptr};
  size_t m_size{0};
};


template <typename T>
class sequence_t {
public:
  constexpr sequence_t(T* const item) noexcept
     : head_(item)
  {
  }

  T* extract() noexcept {
    T* const result = head_;
    head_ = nullptr;
    return result;
  }

  T* pop() noexcept {
    T* const result = head_;
    if (result) {
      head_ = result->next;
      result->next = nullptr;
    }
    return result;
  }

private:
  T* head_{nullptr};
};


template <typename T>
class queue_t {
  using guard = std::lock_guard<std::mutex>;

public:
  sequence_t<T> extract() {
    guard g(m_cs);

    if (m_tail) {
      T* const head = m_tail->next;

      m_tail->next = nullptr;
      m_tail = nullptr;
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
    return m_tail == nullptr;
  }

private:
  T* m_tail{nullptr};
  mutable std::mutex m_cs;
};


/**
 * Используемый алгоритм будет корректно работать, если только
 * единственный поток извлекает объекты из стека и удаляет их.
 */
template <typename T>
class mpsc_stack_t {
public:
  bool empty() const noexcept {
    return (m_head.load() == nullptr);
  }

  sequence_t<T> extract() noexcept {
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

  void push(T* const node) noexcept {
    do {
      T* top = m_head.load();
      node->next = top;
      if (m_head.compare_exchange_weak(top, node)) {
        return;
      }
    } while (true);
  }

  void push(sequence_t<T>&& seq) noexcept {
    while (T* const item = seq.pop()) {
      this->push(item);
    }
  }

  T* pop() noexcept {
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

} // namepsace generics
} // namespace acto
