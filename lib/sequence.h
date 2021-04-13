#pragma once

namespace acto {
namespace generics {

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

} // namespace generics
} // namespace acto
