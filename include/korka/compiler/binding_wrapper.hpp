#pragma once
#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include "korka/utils/function_traits.hpp"
#include "korka/vm/context_base.hpp"
#include "korka/vm/value.hpp"

namespace korka {
  /**
   * The `trap` entry of a bound C++ function: pops its arguments from the VM stack,
   * calls `Func`, pushes the result. Structs are marshalled field by field.
   */
  template<auto Func>
  auto binding_wrapper(vm::context_base &ctx) -> void {
    using function_type = std::remove_cvref_t<decltype(Func)>;
    using traits = function_traits<function_type>;
    using rtype = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;

    constexpr std::size_t arg_slots = vm::tuple_slots_v<args_tuple>;

    // Unpack the arguments from the stack
    auto args = [&]<class ...Ts>(std::type_identity<std::tuple<Ts...>>) {
      constexpr auto offsets = vm::slot_offsets<Ts...>();
      const vm::value *first = ctx.top_slots(arg_slots);

      return [&]<std::size_t ...I>(std::index_sequence<I...>) {
        return args_tuple{vm::read_slots<Ts>(first + offsets[I])...};
      }(std::index_sequence_for<Ts...>());
    }(std::type_identity<args_tuple>{});
    ctx.drop_slots(arg_slots);

    if constexpr (std::is_void_v<rtype>) {
      std::apply(Func, std::move(args));
    } else {
      rtype v = std::apply(Func, std::move(args));
      vm::write_slots<rtype>(v, ctx.push_slots(slot_count_v<rtype>));
    }
  }
}
