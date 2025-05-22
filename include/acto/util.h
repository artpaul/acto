#pragma once

namespace acto::util {

/**
 * This base class is useful for defining move-only events.
 */
struct move_only {
  constexpr move_only() = default;

  constexpr move_only(const move_only&) = delete;
  constexpr move_only(move_only&&) noexcept = default;

  constexpr move_only& operator=(const move_only&) = delete;
  constexpr move_only& operator=(move_only&&) noexcept = default;
};

} // namespace acto::util
