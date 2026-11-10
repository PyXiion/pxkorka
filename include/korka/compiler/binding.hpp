#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "korka/compiler/binding_wrapper.hpp"
#include "korka/shared/struct_def.hpp"
#include "korka/shared/types.hpp"
#include "korka/utils/function_traits.hpp"
#include "korka/vm/context_base.hpp"
#include "korka/vm/op_codes.hpp"

namespace korka {
  template<class... Ts>
  struct type_list {
  };

  namespace detail {
    template<class List, class T>
    struct type_list_contains;

    template<class T, class... Ts>
    struct type_list_contains<type_list<Ts...>, T> : std::bool_constant<(std::is_same_v<T, Ts> or ...)> {
    };

    template<class List, class T>
    struct type_list_append;

    template<class... Ts, class T>
    struct type_list_append<type_list<Ts...>, T> {
      using type = type_list<Ts..., T>;
    };

    // Fields of a bound struct as a list of their types
    template<class Fields>
    struct field_types;

    template<class... Fs>
    struct field_types<std::tuple<Fs...>> {
      template<template<class...> class Target, class List>
      using apply = Target<List, typename Fs::member_type...>;
    };

    template<class List, class... Ts>
    struct add_structs;

    template<class List, class T>
    consteval auto add_struct_result() {
      if constexpr (not bound_struct<T> or type_list_contains<List, T>::value) {
        return std::type_identity<List>{};
      } else {
        using fields_t = std::remove_cvref_t<decltype(struct_def<T>::fields)>;
        using with_fields = typename field_types<fields_t>::template apply<add_structs, List>::type;
        return std::type_identity<typename type_list_append<with_fields, T>::type>{};
      }
    }

    template<class List, class T>
    using add_struct_t = typename decltype(add_struct_result<List, T>())::type;

    template<class List>
    struct add_structs<List> {
      using type = List;
    };

    template<class List, class T, class... Rest>
    struct add_structs<List, T, Rest...> {
      using type = typename add_structs<add_struct_t<List, T>, Rest...>::type;
    };
  }

  template<class T>
  struct expose_tag {
  };

  template<class T>
  consteval auto expose() -> expose_tag<T> {
    static_assert(bound_struct<T>, "Only structs bound with korka::struct_def<T> can be exposed");
    return {};
  }

  template<class Signature>
  struct wrapped_function {
    using signature_t = Signature;

    vm::external_function_type &external_func;
    std::string_view name;
  };

  template<auto func>
  consteval auto wrap(std::string_view name) {
    return wrapped_function<std::decay_t<decltype(func)>>{
      binding_wrapper<func>,
      name
    };
  }

  namespace detail {
    template<class T>
    struct is_wrapped_function : std::false_type {
    };

    template<class Signature>
    struct is_wrapped_function<wrapped_function<Signature>> : std::true_type {
    };

    template<class List, class Item>
    struct add_exposed_by {
      using type = List;
    };

    template<class List, class Signature>
    struct add_exposed_by<List, wrapped_function<Signature>> {
      using traits = function_traits<Signature>;

      template<class>
      struct with_args;

      template<class... Args>
      struct with_args<std::tuple<Args...>> {
        using type = typename add_structs<List, typename traits::return_type, Args...>::type;
      };

      using type = typename with_args<typename traits::args_tuple>::type;
    };

    template<class List, class T>
    struct add_exposed_by<List, expose_tag<T>> {
      using type = add_struct_t<List, T>;
    };

    template<class List, class... Items>
    struct exposed_by_items {
      using type = List;
    };

    template<class List, class Item, class... Rest>
    struct exposed_by_items<List, Item, Rest...> {
      using type = typename exposed_by_items<typename add_exposed_by<List, Item>::type, Rest...>::type;
    };

    template<class T>
    constexpr auto make_struct_info() -> struct_info {
      struct_info info{.name = struct_def<T>::name};
      std::size_t offset = 0;
      std::apply([&](const auto &...f) {
        ([&] {
          using member_t = typename std::remove_cvref_t<decltype(f)>::member_type;
          info.fields.push_back({f.name, cpp_t_to_type_info<member_t>(), offset});
          offset += slot_count_v<member_t>;
        }(), ...);
      }, struct_def<T>::fields);
      info.slot_count = offset;
      return info;
    }

    template<class... Ts>
    constexpr auto make_struct_infos(type_list<Ts...>) -> std::vector<struct_info> {
      std::vector<struct_info> infos;
      (infos.push_back(make_struct_info<Ts>()), ...);
      return infos;
    }
  }

  template<std::size_t NMaxArgs>
  struct function_binding {
    vm::external_function_id id{};
    std::string_view name{};

    std::size_t param_count{};
    std::array<type_info, NMaxArgs> param_types{};

    vm::external_function_type &wrapper;

    type_info return_type;


    template<class Wrapped>
    static consteval auto from_function(vm::external_function_id idx, Wrapped wrapped) -> function_binding {
      using function_type = std::remove_cvref_t<typename Wrapped::signature_t>;
      using traits = function_traits<function_type>;
      using rtype = typename traits::return_type;
      using args = typename traits::args_tuple;

      constexpr std::size_t arg_count = traits::args_count;

      static_assert(arg_count <= NMaxArgs, "Param count mismatch");

      auto ptypes = []<std::size_t ...I>(std::index_sequence<I...>) {
        return std::array<type_info, NMaxArgs>{
          cpp_t_to_type_info<std::tuple_element_t<I, args>>()...
        };
      }(std::make_index_sequence<arg_count>());

      return {
        .id = idx,
        .name = wrapped.name,
        .param_count = arg_count,
        .param_types = ptypes,
        .wrapper = wrapped.external_func,
        .return_type = cpp_t_to_type_info<rtype>()
      };
    }
  };

  /**
   * Everything a script can use from C++: bound functions and bound structs.
   * `Exposed` is the `type_list` of the C++ structs scripts can name, dependencies first.
   */
  template<std::size_t NBindings, std::size_t NMaxArgs, class Exposed = type_list<>>
  class bindings {
    using binding_type = function_binding<NMaxArgs>;
    using array_type = std::array<std::pair<std::string_view, binding_type>, NBindings>;

  public:
    using exposed_types = Exposed;

    constexpr bindings() requires (NBindings == 0) = default;

    constexpr explicit bindings(const array_type &array) : m_entries(array) {
      for (const auto &entry: m_entries) {
        m_callables[entry.second.id] = std::addressof(entry.second.wrapper);
      }
    }

    // compile-time
    constexpr auto get(std::string_view name) const -> std::optional<binding_type> {
      for (const auto &entry: m_entries) {
        if (entry.first == name) return entry.second;
      }
      return std::nullopt;
    }

    constexpr auto contains(std::string_view name) const -> bool {
      return std::ranges::any_of(m_entries, [&](const auto &entry) { return entry.first == name; });
    }

    constexpr auto exposed_structs() const -> std::vector<struct_info> {
      return detail::make_struct_infos(Exposed{});
    }

    // runtime
    auto get_callable_by_id(vm::external_function_id id) const -> vm::external_function_type * {
      return m_callables[id];
    }

  private:
    array_type m_entries{};
    std::array<vm::external_function_type *, NBindings> m_callables{};
  };

  /**
   * Takes `korka::wrap<f>("name")` functions and `korka::expose<T>()` structs.
   * Bound structs used in the signature of a wrapped function are exposed automatically.
   */
  template<class... Items>
  consteval auto make_bindings(Items ...items) {
    constexpr auto func_max_args = [] {
      std::size_t max{};
      ((max = std::max(max, [] {
        if constexpr (detail::is_wrapped_function<Items>::value) {
          return function_traits<typename Items::signature_t>::args_count;
        } else {
          return std::size_t{};
        }
      }())), ...);
      return max;
    }();
    constexpr std::size_t function_count = (std::size_t{} + ... + (detail::is_wrapped_function<Items>::value ? 1 : 0));

    using binding_type = function_binding<func_max_args>;
    using exposed = typename detail::exposed_by_items<type_list<>, Items...>::type;

    auto functions = std::tuple_cat([&] {
      if constexpr (detail::is_wrapped_function<Items>::value) {
        return std::tuple{items};
      } else {
        return std::tuple<>{};
      }
    }()...);

    return std::apply([](auto ...wrapped_functions) {
      vm::external_function_id i{};
      return bindings<function_count, func_max_args, exposed>{
        std::array<std::pair<std::string_view, binding_type>, function_count>{
          std::pair{
            wrapped_functions.name,
            binding_type::from_function(i++, wrapped_functions)
          }...
        }};
    }, functions);
  }
}
