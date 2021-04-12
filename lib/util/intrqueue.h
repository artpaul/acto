#pragma once

#include <assert.h>

namespace acto {
namespace generics {

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

} // namespace generics
} // namespace acto
