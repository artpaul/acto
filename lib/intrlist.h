#pragma once

namespace acto {
namespace core {

/**
 * Интрузивный элемент списка
 */
template <typename T>
struct intrusive_t {
  T* next{nullptr};
};

} // namespace core
} // namespace acto
