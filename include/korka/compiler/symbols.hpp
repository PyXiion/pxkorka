#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>
#include "korka/compiler/info.hpp"
#include "korka/shared/error.hpp"
#include "korka/shared/flat_map.hpp"
#include "korka/shared/types.hpp"

namespace korka {
  struct variable_info {
    std::string_view name{};
    type_info type;
    std::size_t first_slot{};
  };

  /**
   * Script functions plus the variables of the function being compiled.
   * Slots are allocated per function from one running counter; block scopes only hide names,
   * they never give slots back, so the counter ends up as the function's slot count.
   */
  class symbol_table {
  public:
    // Slots of one function are addressed by a 16-bit operand
    static constexpr std::size_t max_slots = 0xFFFF;

    flat_map<std::string_view, function_info> functions;

    constexpr auto begin_function() -> void {
      m_variables.clear();
      m_scope_starts.clear();
      m_slot_count = 0;
    }

    constexpr auto slot_count() const -> std::size_t { return m_slot_count; }

    constexpr auto push_scope() -> void { m_scope_starts.push_back(m_variables.size()); }

    constexpr auto pop_scope() -> void {
      m_variables.resize(m_scope_starts.back());
      m_scope_starts.pop_back();
    }

    constexpr auto declare_var(std::string_view name, const type_info &type, std::size_t slots)
      -> std::expected<variable_info, error_t> {
      for (auto i = m_scope_starts.back(); i < m_variables.size(); ++i) {
        if (m_variables[i].name == name) {
          return std::unexpected{error::redeclaration{.identifier = name}};
        }
      }

      if (m_slot_count + slots > max_slots) {
        return std::unexpected{error::other_compiler_error{"A function uses too many local slots"}};
      }

      variable_info info{.name = name, .type = type, .first_slot = m_slot_count};
      m_slot_count += slots;
      m_variables.push_back(info);
      return info;
    }

    constexpr auto declare_function(function_info info) -> std::expected<void, error_t> {
      if (functions.contains(info.name)) {
        return std::unexpected{error::redeclaration{.identifier = info.name}};
      }
      const auto name = info.name;
      functions.insert(name, std::move(info));
      return {};
    }

    constexpr auto lookup_variable(std::string_view name) const -> std::optional<variable_info> {
      for (auto it = m_variables.rbegin(); it != m_variables.rend(); ++it) {
        if (it->name == name) return *it;
      }
      return std::nullopt;
    }

    constexpr auto lookup_function(std::string_view name) const -> const function_info * {
      if (auto it = functions.find(name); it != functions.end()) {
        return &it->second;
      }
      return nullptr;
    }

  private:
    std::vector<variable_info> m_variables;
    std::vector<std::size_t> m_scope_starts;
    std::size_t m_slot_count{};
  };
}
