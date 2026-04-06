#pragma once

#include <variant>
#include <string_view>
#include <expected>
#include <cstdint>
#include "korka/shared/error.hpp"
#include "korka/shared/types.hpp"
#include "korka/utils/overloaded.hpp"

namespace korka::vm {
  using local_index_t = std::uint8_t;
  using jump_offset = std::int32_t;
  using address_t = std::uint32_t;

  enum class op_code : char {
    // --- Memory & Stack ---
    // Loads a value from the local at index on stack
    // <op><local_index_t>
    lload,

    // Pops a value from stack and saves to the local at index
    // <op><local_index_t>
    lsave,

    // Pushes a value onto the stack
    i64_const, // <op><i64:8>

    // --- Math ---
    // Order:
    // A = pop() # first on stack
    // B = pop() # second on stack
    // C = B / A
    // push(C)

    i64_add,
    i64_sub,
    i64_mul,
    i64_div,

    // push(pop() == pop())
    i64_cmp,

    // --- Control flow ---
    // - Jumps -
    // // <op><jump_offset>
    jmp, // jumps no matter what
    jmpz, // pops value and jumps if it's zero

    // - Other -
    // Call
    // foo(a, b, c)
    // On stack:  arg_count <- top
    //            A <- arguments
    //            B
    //            C
    // Then VM on call puts them into locals
    call, // <op><address_t>
    ret,

    // call external function
    trap, // <op><vm_external_function_id>
  };

  template<korka::type Type>
  constexpr op_code get_const_op_by_type() {
    if constexpr (Type == korka::type::i64) {
      return op_code::i64_const;
    } else {
      static_assert(false, "Unknown type");
    }
  }

  using type_info = std::variant<korka::type>;

  constexpr auto
  get_op_code_for_math(const type_info &ltype, const type_info &rtype, std::string_view op) -> std::expected<op_code, error_t> {
    if (ltype != rtype) {
      return std::unexpected{error::other_error{
        .message = "Math operations between distinct types are not supported yet"
      }};
    }

    return std::visit(overloaded{
      [&](korka::type type) -> std::expected<op_code, error_t> {
        if (type == korka::type::i64) {
          if (op == "+")
            return op_code::i64_add;
          if (op == "-")
            return op_code::i64_sub;
          if (op == "*")
            return op_code::i64_mul;
          if (op == "/")
            return op_code::i64_div;
          return std::unexpected{error::unsupported_math_op{
            .type = "i64",
            .op = op
          }};
        }
        return std::unexpected{error::other_error{
          .message = "Unsupported type for math"
        }};
      },
      [&](auto &&) -> std::expected<op_code, error_t> {

        return std::unexpected{error::other_error{
          .message = "Unsupported type for math"
        }};
      }
    }, ltype);
  }

  constexpr int op_code_size = 1;

  template<auto getter>
  constexpr auto _type_info_to_cpp() {
    if constexpr (std::holds_alternative<korka::type>(getter())) {
      constexpr static korka::type t = [] { return std::get<korka::type>(getter()); }();
      using type = type_to_cpp_t<t>;
      if constexpr (std::is_void_v<type>) {
        return;
      } else {
        return std::decay_t<type>{};
      }
    } else {
      static_assert(false, "Unsupported type");
    }
  }

  template<auto type_info_getter>
  using type_info_to_cpp_t = decltype(_type_info_to_cpp<type_info_getter>());


  template<class T>
  constexpr auto cpp_t_to_type_info() -> type_info {
    if constexpr (std::is_same_v<T, void>) {
      return {type::void_};
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
      return {type::i64};
    } else {
      static_assert(false, "Unsupported type");
    }
  }
}