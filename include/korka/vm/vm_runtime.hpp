//
// Created by pyxiion on 29.01.2026.
//

#pragma once

#include "korka/utils/function_traits.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/utils/byte_reader.hpp"
#include "korka/compiler/result.hpp"
#include "korka/vm/value.hpp"
#include "korka/compiler/binding.hpp"
#include "korka/vm/context_base.hpp"
#include <array>
#include <bit>
#include <vector>
#include <cstdint>

namespace korka::vm {
  template<bindings_concepts bindings_t = bindings<0, 0>>
  class context : public context_base {
  public:
    explicit context(std::span<const std::byte> bytes, bindings_t binds = {})
      : context_base(bytes), m_bindings(binds) {

    }

    template<class Signature, class ...Args, class Traits = function_traits<Signature>>
    auto call(function_runtime_info_with_signature<Signature> func, Args &&...args) -> typename Traits::return_type {
      format_static_assert<Traits::args_count == sizeof...(args), [] {
        return format("This function requires ~ arguments, got ~", Traits::args_count, sizeof...(args));
      }>();

      // Create a scope for the call
      push_scope();

      // Pass arguments
      std::size_t i = 0;
      ([&]() {
        current_scope()->set_local_value(i++, box(args));
      }(), ...);

      m_reader.set_cursor(func.start_pos);

      execute_loop();

      using return_type = typename Traits::return_type;
      if constexpr (std::is_void_v<return_type>) {
        return;
      } else {
        return pop<return_type>();
      }
    }

  protected:
    bindings_t m_bindings;

    void execute_loop() {
      const std::byte *ip = m_reader.data() + m_reader.cursor();

      // Using macros instead of lambdas, because
      // templates with lambdas (like lambda<T>) don't work
      // well
#define READ(T)   (*reinterpret_cast<const T*>(ip)); ip += sizeof(T)
#define IP_POS()  static_cast<std::size_t>(ip - m_reader.data())

      // Using dispatch table instead of switch case, because it's
      // much faster for the CPU
      // Also instead of calling execute_op every time I do it right here
      // without extra calls

      // Also using C99 extensions, I should get rid of them later.
      static const void *const dispatch[] = {
        [int(op_code::lload)]     = &&op_lload,
        [int(op_code::lsave)]     = &&op_lsave,
        [int(op_code::i64_const)] = &&op_i64_const,
        [int(op_code::i64_add)]   = &&op_i64_add,
        [int(op_code::i64_sub)]   = &&op_i64_sub,
        [int(op_code::i64_mul)]   = &&op_i64_mul,
        [int(op_code::i64_div)]   = &&op_i64_div,
        [int(op_code::i64_cmp)]   = &&op_i64_cmp,
        [int(op_code::jmp)]       = &&op_jmp,
        [int(op_code::jmpz)]      = &&op_jmpz,
        [int(op_code::call)]      = &&op_call,
        [int(op_code::ret)]       = &&op_ret,
        [int(op_code::trap)]      = &&op_trap,
      };

      op_start:
      const std::size_t instr_start = IP_POS();
      const op_code code = READ(op_code);

      // @formatter:off
      // clang-format off
      goto *dispatch[int(code)];
      // clang-format on
      // @formatter:on

      op_lload:
      {
        const auto index = READ(local_index_t);
        push_value(current_scope()->get_local_value(index));
        goto op_start;
      }

      op_lsave:
      {
        const auto index = READ(local_index_t);
        current_scope()->set_local_value(index, pop_value());
        goto op_start;
      }

      op_i64_const:
      {
        const auto v = READ(std::int64_t);
        push(v);
        goto op_start;
      }

      op_i64_add:
      {
        const auto b = pop<std::int64_t>();
        const auto a = pop<std::int64_t>();
        push(a + b);
        goto op_start;
      }
      op_i64_sub:
      {
        const auto b = pop<std::int64_t>();
        const auto a = pop<std::int64_t>();
        push(a - b);
        goto op_start;
      }
      op_i64_mul:
      {
        const auto b = pop<std::int64_t>();
        const auto a = pop<std::int64_t>();
        push(a * b);
        goto op_start;
      }
      op_i64_div:
      {
        const auto b = pop<std::int64_t>();
        const auto a = pop<std::int64_t>();
        push(a / b);
        goto op_start;
      }
      op_i64_cmp:
      {
        const auto b = pop<std::int64_t>();
        const auto a = pop<std::int64_t>();
        push(static_cast<std::int64_t>(a == b));
        goto op_start;
      }

      op_jmp:
      {
        const auto offset = READ(jump_offset);
        ip = m_reader.data() + (instr_start + offset);
        goto op_start;
      }

      op_jmpz:
      {
        const auto offset = READ(jump_offset);
        const auto v = pop<std::int64_t>();
        if (__builtin_expect(v == 0, 0)) {
          ip = m_reader.data() + (instr_start + offset);
        }
        goto op_start;
      }

      op_call:
      {
        auto called_address = READ(address_t);
        current_scope()->suspension_point = IP_POS();
        auto arg_count = pop<type::i64>();

        push_scope();
        for (int i = arg_count - 1; i >= 0; --i) {
          current_scope()->set_local_value(i, pop_value());
        }
        ip = m_reader.data() + called_address;
        goto op_start;
      }

      op_ret:
      {
        pop_scope();
        if (current_scope()) {
          ip = m_reader.data() + current_scope()->suspension_point;
        } else {
          m_scopes.clear();
          m_reader.set_cursor(IP_POS());
          return;
        }
        goto op_start;
      }

      op_trap:
      {
        auto id = READ(vm_external_function_id);
        auto func = m_bindings.get_callable_by_id(id);
        (*func)(*this);
        goto op_start;
      }
    }

#undef READ
#undef IP_POS
  };
} // korka::vm