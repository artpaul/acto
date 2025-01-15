#pragma once

#include <atomic>
#include <mutex>

namespace acto::intrusive {

/// Intrusive node.
template <typename T>
struct node {
  T* next{nullptr};
};

template <typename T>
class sequence {
public:
  constexpr sequence(T* const item) noexcept
    : head_{item} {
  }

  constexpr sequence(sequence<T>&& other) noexcept
    : head_{nullptr} {
    std::swap(head_, other.head_);
  }

  constexpr T* extract() noexcept {
    T* result = head_;
    head_ = nullptr;
    return result;
  }

  constexpr T* pop_front() noexcept {
    T* result = head_;
    if (result) {
      head_ = result->next;
      result->next = nullptr;
    }
    return result;
  }

private:
  T* head_;
};

template <typename T, typename Mutex = std::mutex>
class queue {
public:
  sequence<T> extract() {
    std::lock_guard g(mutex_);

    if (tail_) {
      T* head = tail_->next;

      tail_->next = nullptr;
      tail_ = nullptr;
      return sequence<T>(head);
    }
    return nullptr;
  }

  void push(T* const node) {
    std::lock_guard g(mutex_);

    if (tail_) {
      node->next = tail_->next;
      tail_->next = node;
    } else {
      node->next = node;
    }
    tail_ = node;
  }

  T* pop() {
    std::lock_guard g(mutex_);

    if (tail_) {
      T* result = tail_->next;

      if (result == tail_)
        tail_ = nullptr;
      else
        tail_->next = tail_->next->next;

      result->next = nullptr;
      return result;
    }
    return nullptr;
  }

  bool empty() const {
    std::lock_guard g(mutex_);

    return tail_ == nullptr;
  }

private:
  T* tail_{nullptr};
  mutable Mutex mutex_;
};

/**
 * Multiple producers single consumer lock-free stack.
 */
template <typename T>
class mpsc_stack {
public:
  bool empty() const noexcept {
    return (head_.load(std::memory_order_relaxed) == nullptr);
  }

  sequence<T> extract() noexcept {
    do {
      T* top = head_.load();

      if (top == nullptr) {
        return sequence<T>{nullptr};
      }
      if (head_.compare_exchange_weak(top, nullptr)) {
        return sequence<T>{top};
      }
    } while (true);
  }

  void push(T* const node) noexcept {
    do {
      T* top = head_.load();
      node->next = top;
      if (head_.compare_exchange_weak(top, node)) {
        return;
      }
    } while (true);
  }

  void push(sequence<T>&& seq) noexcept {
    while (T* const item = seq.pop()) {
      this->push(item);
    }
  }

  T* pop() noexcept {
    do {
      T* top = head_.load();

      if (top == nullptr) {
        return nullptr;
      } else {
        T* next = top->next;

        if (head_.compare_exchange_weak(top, next)) {
          top->next = nullptr;
          return top;
        }
      }
    } while (true);
  }

private:
  std::atomic<T*> head_{nullptr};
};

/**
 * Simple intrusive stack without locks.
 */
template <typename T>
class stack {
public:
  constexpr stack() noexcept = default;

  constexpr stack(sequence<T>&& seq) noexcept
    : head_(seq.extract()) {
  }

  constexpr bool empty() const noexcept {
    return !head_;
  }

  constexpr sequence<T> extract() noexcept {
    T* top = head_;
    head_ = nullptr;
    return sequence<T>{top};
  }

  constexpr void push(T* const node) noexcept {
    node->next = head_;
    head_ = node;
  }

  constexpr void push(sequence<T>&& seq) noexcept {
    while (T* const item = seq.pop_front()) {
      this->push(item);
    }
  }

  constexpr T* pop() noexcept {
    T* result = head_;

    if (result) {
      head_ = result->next;
      result->next = nullptr;
    }
    return result;
  }

private:
  T* head_{nullptr};
};

} // namespace acto::intrusive
