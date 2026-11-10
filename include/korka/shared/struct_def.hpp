#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include "korka/shared/types.hpp"

#ifdef __cpp_impl_reflection
#include <meta>
#endif

namespace korka {
  /**
   * Describes one data member of a bound struct: `korka::field{"x", &Vec::x}`.
   * The member is either `std::int64_t` or another bound struct.
   */
  template<class C, class M>
  struct field {
    using owner_type = C;
    using member_type = M;

    std::string_view name;
    M C::*ptr;

    constexpr field(std::string_view name_, M C::*ptr_) : name(name_), ptr(ptr_) {}
  };

  template<class C, class M>
  field(std::string_view, M C::*) -> field<C, M>;

  /**
   * The single customisation point that exposes a C++ struct to scripts:
   *
   *   template<> struct korka::struct_def<Vec> {
   *     static constexpr std::string_view name = "Vec";
   *     static constexpr auto fields = std::tuple{ korka::field{"x", &Vec::x}, ... };
   *   };
   *
   * With C++26 reflection (`__cpp_impl_reflection`) aggregates get a default definition;
   * an explicit specialisation always wins.
   */
  template<class T>
  struct struct_def;

  namespace detail {
    template<class T>
    consteval auto is_bound_struct() -> bool;

    template<class T, class F>
    struct is_valid_field : std::false_type {
    };

    template<class T, class M>
    struct is_valid_field<T, field<T, M>>
        : std::bool_constant<std::is_same_v<M, std::int64_t> or is_bound_struct<M>()> {
    };

    template<class T, class... Fs>
    consteval auto are_valid_fields(const std::tuple<Fs...> *) -> bool {
      return (is_valid_field<T, Fs>::value and ... and true);
    }

    template<class T>
    consteval auto is_bound_struct() -> bool {
      if constexpr (std::is_class_v<T> and requires {
        struct_def<T>::name;
        struct_def<T>::fields;
      }) {
        using fields_t = std::remove_cvref_t<decltype(struct_def<T>::fields)>;
        return std::default_initializable<T> and are_valid_fields<T>(static_cast<const fields_t *>(nullptr));
      } else {
        return false;
      }
    }
  }

  template<class T>
  concept bound_struct = detail::is_bound_struct<T>();

  /**
   * How many VM slots a C++ type takes: `void` 0, `std::int64_t` and `double` 1,
   * a bound struct is the sum of its fields.
   */
  template<class T>
  constexpr auto slot_count() -> std::size_t {
    if constexpr (std::is_void_v<T>) {
      return 0;
    } else if constexpr (std::is_same_v<T, std::int64_t> or std::is_same_v<T, double>) {
      return 1;
    } else if constexpr (bound_struct<T>) {
      return std::apply([](const auto &...f) {
        return (std::size_t{} + ... + slot_count<typename std::remove_cvref_t<decltype(f)>::member_type>());
      }, struct_def<T>::fields);
    } else {
      static_assert(false, "Unsupported type: bind a struct by specialising korka::struct_def<T>");
    }
  }

  template<class T>
  inline constexpr std::size_t slot_count_v = slot_count<T>();

  template<class T>
  constexpr auto cpp_t_to_type_info() -> type_info {
    if constexpr (std::is_void_v<T>) {
      return type_info::void_();
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
      return type_info::i64();
    } else if constexpr (bound_struct<T>) {
      return type_info::structure(struct_def<T>::name);
    } else {
      static_assert(false,
                    "Unsupported type: use std::int64_t, void, or a struct bound by specialising korka::struct_def<T>");
    }
  }

#ifdef __cpp_impl_reflection
  template<class T> requires (std::is_class_v<T> and std::is_aggregate_v<T>)
  struct struct_def<T> {
    static constexpr std::string_view name = std::meta::identifier_of(^^T);

    static constexpr auto members = std::define_static_array(
      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));

    static constexpr auto fields = []<std::size_t... I>(std::index_sequence<I...>) {
      return std::tuple{
        field<T, typename [:std::meta::type_of(members[I]):]>{
          std::meta::identifier_of(members[I]),
          &[:members[I]:]
        }...
      };
    }(std::make_index_sequence<members.size()>{});
  };
#endif
}
