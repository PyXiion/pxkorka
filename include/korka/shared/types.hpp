#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace korka {
  enum class type {
    void_,
    i64
  };

  namespace detail {
    template<type>
    struct type_to_cpp_;
    template<>
    struct type_to_cpp_<type::void_> {
      using type = void;
    };

    template<>
    struct type_to_cpp_<type::i64> {
      using type = std::int64_t;
    };
  }

  template<type T>
  using type_to_cpp_t = typename detail::type_to_cpp_<T>::type;

  /**
   * Either a builtin type or a reference to a struct. Struct typing is nominal:
   * two struct types are equal when their names are equal.
   */
  struct type_info {
    type builtin{type::void_};
    std::string_view struct_name{}; // non-empty => this is a struct reference

    static constexpr auto void_() -> type_info { return {}; }

    static constexpr auto i64() -> type_info { return {.builtin = type::i64}; }

    static constexpr auto structure(std::string_view name) -> type_info { return {.struct_name = name}; }

    constexpr auto is_struct() const -> bool { return not struct_name.empty(); }

    constexpr auto is_void() const -> bool { return not is_struct() and builtin == type::void_; }

    constexpr auto is_i64() const -> bool { return not is_struct() and builtin == type::i64; }

    friend constexpr auto operator==(const type_info &l, const type_info &r) -> bool {
      if (l.is_struct() or r.is_struct()) return l.struct_name == r.struct_name;
      return l.builtin == r.builtin;
    }
  };

  constexpr auto type_name(const type_info &t) -> std::string_view {
    if (t.is_struct()) return t.struct_name;
    return t.builtin == type::i64 ? "int" : "void";
  }

  struct field_info {
    std::string_view name;
    type_info type;
    std::size_t slot_offset;
  };

  /**
   * A struct is a compile-time view over `slot_count` consecutive VM slots;
   * nested structs are laid out inline.
   */
  struct struct_info {
    std::string_view name;
    std::vector<field_info> fields;
    std::size_t slot_count{};
  };
}
