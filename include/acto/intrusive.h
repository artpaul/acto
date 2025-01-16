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

class spin_lock {
public:
  void lock() noexcept {
    while (flag_.test_and_set(std::memory_order_acquire)) {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
      flag_.wait(true, std::memory_order_relaxed);
#endif
    }
  }

  void unlock() noexcept {
    flag_.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
    flag_.notify_one();
#endif
  }

private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

template <typename T, typename Mutex = spin_lock>
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

  /// @brief Appends a node in the queue.
  /// @returns true if the queue was empty.
  bool push(T* const node) {
    std::lock_guard g(mutex_);

    if (tail_) {
      node->next = tail_->next;
      tail_->next = node;
    } else {
      node->next = node;
    }
    tail_ = node;
    return node->next == node;
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
      T* top = head_.load(std::memory_order_relaxed);

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
      T* top = head_.load(std::memory_order_relaxed);
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
      T* top = head_.load(std::memory_order_consume);
      // Check the stack is empty.
      if (top == nullptr) {
        return nullptr;
      }

      T* next = top->next;
      // Pop the item.
      if (head_.compare_exchange_weak(top, next)) {
        top->next = nullptr;
        return top;
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
