#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "korka/compiler/binding.hpp"
#include "korka/compiler/info.hpp"
#include "korka/shared/error.hpp"
#include "korka/shared/flat_map.hpp"
#include "korka/shared/struct_def.hpp"
#include "korka/utils/string.hpp"
#include "korka/vm/function_handle.hpp"

namespace korka {
  template<std::size_t NMaxParams>
  constexpr auto function_info_to_const(const function_info &f) {
    const_function_info<NMaxParams> info{
      .name = f.name,
      .param_count = f.param_types.size(),
      .param_types{},
      .return_type = f.return_type,
      .start_pos = f.start_pos
    };

    std::ranges::copy(f.param_types, std::begin(info.param_types));
    return info;
  }

  struct compilation_result {
    std::vector<std::byte> bytes;
    flat_map<std::string_view, function_info> functions;
  };

  // The functions of a script. Few and looked up rarely, so a plain scan: building a perfect hash
  // costs too many constexpr steps.
  template<std::size_t NFunctions, std::size_t NMaxParams>
  struct const_function_table {
    std::array<const_function_info<NMaxParams>, NFunctions> items;

    constexpr auto find(std::string_view name) const -> const const_function_info<NMaxParams> * {
      for (const auto &f: items) {
        if (f.name == name) return &f;
      }
      return nullptr;
    }
  };

  namespace detail {
    // C++ struct types that scripts can name, by script name
    template<class Exposed>
    struct exposed_lookup;

    template<class... Ts>
    struct exposed_lookup<type_list<Ts...>> {
      static constexpr std::size_t npos = sizeof...(Ts);

      static consteval auto index_of(std::string_view name) -> std::size_t {
        std::size_t i = 0;
        std::size_t found = npos;
        ((found == npos and struct_def<Ts>::name == name ? found = i : 0, ++i), ...);
        return found;
      }

      template<std::size_t I>
      using at = std::tuple_element_t<I, std::tuple<Ts...>>;
    };

    // The C++ type of a script type. Only reached for functions that are actually exported,
    // a script-only struct is an error there and nowhere else.
    template<auto type_getter, class Exposed>
    consteval auto resolve_cpp_type() {
      constexpr type_info t = type_getter();
      if constexpr (t.is_struct()) {
        using lookup = exposed_lookup<Exposed>;
        constexpr auto index = lookup::index_of(t.struct_name);
        if constexpr (index == lookup::npos) {
          report_error<[] {
            return korka::format("The signature uses the script-only struct '~', it has no C++ counterpart",
                                 type_getter().struct_name);
          }>();
          return std::type_identity<std::int64_t>{}; // placeholder, the error is already reported
        } else {
          return std::type_identity<typename lookup::template at<index>>{};
        }
      } else {
        return std::type_identity<type_to_cpp_t<t.builtin>>{};
      }
    }

    template<auto type_getter, class Exposed>
    using cpp_type_t = typename decltype(resolve_cpp_type<type_getter, Exposed>())::type;

    template<class R, class... Args>
    using function_type_t = R(Args...);

    template<auto info_getter, class Exposed, class IndexSequence>
    struct signature_of;

    template<auto info_getter, class Exposed, std::size_t... Is>
    struct signature_of<info_getter, Exposed, std::index_sequence<Is...>> {
      using type = function_type_t<
        cpp_type_t<[] { return info_getter().return_type; }, Exposed>,
        cpp_type_t<[] { return info_getter().param_types[Is]; }, Exposed>...
      >;
    };

    template<auto functions_getter, class Exposed, const_string name>
    consteval auto resolve_signature() {
      constexpr bool found = functions_getter().find(std::string_view{name}) != nullptr;
      if constexpr (not found) {
        report_error<[] {
          return korka::format("Function '~' is not defined in the script", std::string_view{name});
        }>();
        return std::type_identity<void()>{};
      } else {
        constexpr auto info_getter = [] { return *functions_getter().find(std::string_view{name}); };
        return std::type_identity<typename signature_of<
          info_getter, Exposed, std::make_index_sequence<info_getter().param_count>
        >::type>{};
      }
    }
  }

  template<std::size_t NBytes, std::size_t NFunctions, std::size_t NMaxParams,
    auto functions_getter, class Exposed>
  struct const_compilation_result {
    std::array<std::byte, NBytes> bytes;
    const_function_table<NFunctions, NMaxParams> functions;

    // C++ signature of a script function. Fails to compile if the function does not exist or
    // its signature uses a struct that is not bound.
    template<const_string name>
    using get_signature_t = typename decltype(detail::resolve_signature<functions_getter, Exposed, name>())::type;

    template<const_string name>
    constexpr auto function() const {
      return vm::function_handle<get_signature_t<name>>{
        functions.find(std::string_view{name})->start_pos
      };
    }
  };

  template<auto r, class Exposed>
  constexpr auto compilation_result_to_const() {
    constexpr static auto sizes = []() constexpr {
      auto result = r();
      std::size_t max_params{};
      for (auto &&f: result.functions) {
        max_params = std::max(max_params, f.second.param_types.size());
      }
      return std::array<std::size_t, 3>{result.bytes.size(), result.functions.size(), max_params};
    }();
    constexpr static std::size_t byte_count = sizes[0];
    constexpr static std::size_t function_count = sizes[1];
    constexpr static std::size_t max_params_n = sizes[2];

    constexpr static auto bytes = []() constexpr {
      auto result = r();
      std::array<std::byte, byte_count> out{};
      std::ranges::copy(result.bytes, out.begin());
      return out;
    }();

    constexpr static auto functions = []() constexpr {
      auto result = r();
      const_function_table<function_count, max_params_n> table{};
      std::size_t i{};
      for (auto &&entry: result.functions) {
        table.items[i++] = function_info_to_const<max_params_n>(entry.second);
      }
      return table;
    }();

    return const_compilation_result<byte_count, function_count, max_params_n, [] { return functions; }, Exposed>{
      bytes,
      functions
    };
  }
}
