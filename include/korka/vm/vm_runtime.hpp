//
// Created by pyxiion on 29.01.2026.
//

#pragma once

#include "korka/utils/function_traits.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/utils/byte_reader.hpp"
#include "korka/compiler/compiler.hpp"
#include <array>
#include <bit>
#include <vector>
#include <cstdint>

namespace korka::vm {
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

  constexpr auto box_int(std::int64_t i) -> value {
    return std::bit_cast<value>(i);
  }

  class context {
  public:
    explicit context(std::span<const std::byte> bytes)
      : m_reader(bytes) {
      m_stack.reserve(64);
      m_locals.resize(8);
    }

//    auto run(function_info const& fi) {
//
//    }

    template<class Signature, class ...Args, class Traits = function_traits<Signature>>
    auto run(function_runtime_info_with_signature<Signature>, Args &&...args) -> typename Traits::return_type {

      format_static_assert<Traits::args_count == sizeof...(args), [] {
        return format("This function requires ~ arguments, got ~", Traits::args_count, sizeof...(args));
      }>();

      // TODO: check argument types

      // Pass arguments
      std::size_t i = 0;
      ([&]() {
        set_local_value(i++, box(args));
      }(), ...);

      // Run until ret
      while (true) {
        auto op = execute_op();

        if (op == op_code::ret) {
          break;
        }
      }

      return pop<typename Traits::return_type>();
    }

    auto push_value(value const &v) -> void {
      m_stack.emplace_back(v);
    }

    template<class T>
    auto push(T const &v) -> void {
      push_value(box<T>(v));
    }

    auto pop_value() -> value {
      value v = m_stack.back();
      m_stack.pop_back();
      return v;
    }

    template<class T>
    auto pop() -> T {
      return unbox<T>(pop_value());
    }

    auto set_local_value(local_index_t i, value const &v) -> void {
      m_locals.at(i) = v;
    }

    template<class T = value>
    auto set_local(local_index_t i, T const &value) -> void {
      set_local_value(i, box<T>(value));
    }

    auto get_local_value(local_index_t i) -> value {
      return m_locals.at(i);
    }

    template<class T = value>
    auto get_local(local_index_t i) -> T {
      return unbox<T>(get_local_value(i));
    }

  private:
    auto execute_op() -> op_code {
      auto initial_pc = m_reader.cursor();

      const op_code code = m_reader.read<op_code>();

      switch (code) {
        case op_code::lload: {
          const auto index = m_reader.read<local_index_t>();
          push_value(get_local_value(index));
        }
          break;
        case op_code::pload:
          throw std::runtime_error("Not implemented");
          break;
        case op_code::lsave: {
          const auto index = m_reader.read<local_index_t>();
          set_local_value(index, pop_value());
        }
          break;
        case op_code::i64_const: {
          const auto v = m_reader.read<std::int64_t>();
          push(v);
        }
          break;
        case op_code::i64_add: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a + b);
        }
          break;
        case op_code::i64_sub: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a - b);
        }
          break;
        case op_code::i64_mul: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a * b);
        }
          break;
        case op_code::i64_div: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a / b);
        }
          break;
        case op_code::jmp: {
          const auto offset = m_reader.read<jump_offset>();
          m_reader.set_cursor(initial_pc + offset);
        }
          break;
        case op_code::jmpz: {
          const auto offset = m_reader.read<jump_offset>();
          auto v = pop<std::int64_t>();
          if (v == 0) {
            m_reader.set_cursor(initial_pc + offset);
          }
        }
          break;
        case op_code::ret:
          // TODO
          // Works for now, since we don't have function calls inside the VM
          break;
      }

      return code;
    }

    std::vector<value> m_stack;
    std::vector<value> m_locals;

    byte_reader m_reader;
  };

  class runtime {
  public:
//    auto create_context() -> context {
//      return {};
//    }


  };

} // korka::vm