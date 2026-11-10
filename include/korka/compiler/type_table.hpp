#pragma once

#include <expected>
#include <optional>
#include <string_view>
#include <vector>
#include "korka/shared/error.hpp"
#include "korka/shared/types.hpp"

namespace korka {
  /**
   * All struct types visible to a script: the bound ones first, then the ones the script declares.
   */
  class type_table {
  public:
    constexpr auto contains_struct(std::string_view name) const -> bool {
      return find_struct(name) != nullptr;
    }

    /**
     * Returns false if the name is taken.
     */
    constexpr auto add(struct_info info) -> bool {
      if (contains_struct(info.name)) return false;
      m_structs.push_back(std::move(info));
      return true;
    }

    /**
     * `int`, `void` or a declared struct.
     */
    constexpr auto lookup(std::string_view name) const -> std::expected<type_info, error_t> {
      if (name == "int") return type_info::i64();
      if (name == "void") return type_info::void_();
      if (contains_struct(name)) return type_info::structure(name);
      return std::unexpected{error::unknown_type{name}};
    }

    constexpr auto slot_count(const type_info &t) const -> std::size_t {
      if (t.is_struct()) return find_struct(t.struct_name)->slot_count;
      return t.is_i64() ? 1 : 0;
    }

    /**
     * Looks a field up in a struct type; nullopt if it has no such field.
     */
    constexpr auto find_field(const type_info &t, std::string_view field_name) const -> std::optional<field_info> {
      for (const auto &f: find_struct(t.struct_name)->fields) {
        if (f.name == field_name) return f;
      }
      return std::nullopt;
    }

  private:
    std::vector<struct_info> m_structs;

    constexpr auto find_struct(std::string_view name) const -> const struct_info * {
      for (const auto &s: m_structs) {
        if (s.name == name) return &s;
      }
      return nullptr;
    }
  };
}
