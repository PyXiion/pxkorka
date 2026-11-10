//
// Created by pyxiion on 20/03/2026.
//

#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include "korka/shared/struct_def.hpp"

namespace korka::vm {
  // An untyped slot
  using value = std::array<std::byte, 8>;

  template<class T>
  constexpr auto box(T const &v) -> value {
    static_assert(std::is_same_v<T, std::int64_t> or std::is_same_v<T, double>, "T must be int or double");

    return std::bit_cast<value>(v);
  }

  template<class T>
  constexpr auto unbox(value const &v) -> T {
    static_assert(std::is_same_v<T, std::int64_t> or std::is_same_v<T, double>, "T must be int or double");

    return std::bit_cast<T>(v);
  }

  /**
   * Writes `v` to `slot_count_v<T>` slots, field by field in `struct_def<T>::fields` order,
   * nested structs inline.
   */
  template<class T>
  constexpr auto write_slots(const T &v, value *out) -> void {
    if constexpr (std::is_same_v<T, std::int64_t> or std::is_same_v<T, double>) {
      *out = box(v);
    } else if constexpr (bound_struct<T>) {
      std::apply([&](const auto &...f) {
        ([&] {
          using member_t = typename std::remove_cvref_t<decltype(f)>::member_type;
          write_slots<member_t>(v.*(f.ptr), out);
          out += slot_count_v<member_t>;
        }(), ...);
      }, struct_def<T>::fields);
    } else {
      static_assert(false, "Unsupported type: bind a struct by specialising korka::struct_def<T>");
    }
  }

  /**
   * Reads `slot_count_v<T>` slots written by `write_slots`.
   */
  template<class T>
  constexpr auto read_slots(const value *in) -> T {
    if constexpr (std::is_same_v<T, std::int64_t> or std::is_same_v<T, double>) {
      return unbox<T>(*in);
    } else if constexpr (bound_struct<T>) {
      T result{};
      std::apply([&](const auto &...f) {
        ([&] {
          using member_t = typename std::remove_cvref_t<decltype(f)>::member_type;
          result.*(f.ptr) = read_slots<member_t>(in);
          in += slot_count_v<member_t>;
        }(), ...);
      }, struct_def<T>::fields);
      return result;
    } else {
      static_assert(false, "Unsupported type: bind a struct by specialising korka::struct_def<T>");
    }
  }

  /**
   * Slot offset of every type in `Ts...` when they are laid out one after another.
   */
  template<class... Ts>
  constexpr auto slot_offsets() -> std::array<std::size_t, sizeof...(Ts)> {
    std::array<std::size_t, sizeof...(Ts)> offsets{};
    std::size_t next = 0;
    std::size_t i = 0;
    ((offsets[i++] = next, next += slot_count_v<Ts>), ...);
    return offsets;
  }

  template<class Tuple>
  struct tuple_slots;

  /**
   * Total slot count of the types of a `std::tuple<Ts...>`.
   */
  template<class... Ts>
  struct tuple_slots<std::tuple<Ts...>> {
    static constexpr std::size_t count = (std::size_t{} + ... + slot_count_v<Ts>);
  };

  template<class Tuple>
  inline constexpr std::size_t tuple_slots_v = tuple_slots<Tuple>::count;
}
