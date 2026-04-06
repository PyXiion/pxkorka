#pragma once

#include "frozen/unordered_map.h"
#include "info.hpp"
#include "korka/utils/function_traits.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/vm/value.hpp"
#include "korka/compiler/binding_wrapper.hpp"
#include <utility>
#include <array>

namespace korka {
  template<std::size_t NMaxArgs>
  struct function_binding {
    vm_external_function_id id{};
    std::string_view name{};

    std::size_t param_count{};
    std::array<vm::type_info, NMaxArgs> param_types{};

    vm_external_function_type &wrapper;

    vm::type_info return_type;


    template<class Wrapped>
    static consteval auto from_function(vm_external_function_id idx, Wrapped wrapped) -> function_binding {
      using function_type = std::remove_cvref_t<typename Wrapped::signature_t>;
      using traits = function_traits<function_type>;
      using rtype = typename traits::return_type;
      using args = typename traits::args_tuple;

      constexpr std::size_t arg_count = traits::args_count;

      static_assert(arg_count <= NMaxArgs, "Param count mismatch");

      auto ptypes = []<std::size_t ...I>(std::index_sequence<I...>) {
        return std::array<vm::type_info, NMaxArgs>{
          vm::cpp_t_to_type_info<std::tuple_element_t<I, args>>()...
        };
      }(std::make_index_sequence<arg_count>());

      return {
        .id = idx,
        .name = wrapped.name,
        .param_count = arg_count,
        .param_types = ptypes,
        .wrapper = wrapped.external_func,
        .return_type = vm::cpp_t_to_type_info<rtype>()
      };
    }
  };

  template<std::size_t NBindings, std::size_t NMaxArgs>
  class bindings {
    using binding_type = function_binding<NMaxArgs>;
    using array_type = std::array<std::pair<std::string_view, binding_type>, NBindings>;
    using map_type = frozen::unordered_map<std::string_view, binding_type, NBindings>;

  public:
    constexpr explicit bindings(const array_type &array)
      : m_map(frozen::make_unordered_map(array)) {}

    constexpr auto get(std::string_view name) -> std::optional<binding_type> {
      if (not m_map.contains(name)) {
        return std::nullopt;
      }
      return m_map.at(name);
    }

    // runtime
    auto get_callable_by_id(vm_external_function_id id) const -> vm_external_function_type * {
      for (auto &&[k, v]: m_map) {
        if (v.id == id) {
          return std::addressof(v.wrapper);
        }
      }
      return nullptr;
    }

  private:
    map_type m_map;
  };

  template<std::size_t NMaxArgs>
  class bindings<0, NMaxArgs> {
    using binding_type = function_binding<NMaxArgs>;
    using array_type = std::array<std::pair<std::string_view, binding_type>, 0>;

  public:
    constexpr explicit bindings() = default;

    constexpr explicit bindings(const array_type &) {};

    constexpr auto get(std::string_view) -> std::optional<binding_type> {
      return std::nullopt;
    }

    auto get_callable_by_id(vm_external_function_id) const -> vm_external_function_type * {
      return nullptr;
    }
  };

  consteval auto make_bindings(auto ...wrapped_functions) {
    constexpr auto func_max_args = [] {
      std::size_t max{};
      ((max = std::max(max, function_traits<
        typename std::decay_t<decltype(wrapped_functions)>::signature_t
      >::args_count)), ...);
      return max;
    }();

    using binding_type = function_binding<func_max_args>;

    vm_external_function_id i{};
    return bindings<sizeof...(wrapped_functions), func_max_args>{
      std::array<std::pair<std::string_view, binding_type>, sizeof...(wrapped_functions)>{
        std::pair{
          wrapped_functions.name,
          binding_type::from_function(i++, wrapped_functions)
        }...
      }};
  }

  template<class Signature>
  struct wrapped_function {
    using signature_t = Signature;

    vm_external_function_type &external_func;
    std::string_view name;
  };

  template<auto func>
  consteval auto wrap(std::string_view name) {
    return wrapped_function<std::decay_t<decltype(func)>>{
      binding_wrapper<func>,
      name
    };
  }

}