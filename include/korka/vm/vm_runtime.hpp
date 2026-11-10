//
// Created by pyxiion on 29.01.2026.
//

#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <tuple>
#include <utility>
#include "korka/shared/error.hpp"
#include "korka/utils/function_traits.hpp"
#include "korka/vm/context_base.hpp"
#include "korka/vm/function_handle.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/vm/value.hpp"

namespace korka::vm {
  /**
   * Bindings of a context that has no bound functions.
   */
  struct empty_bindings {
    auto get_callable_by_id(external_function_id) const -> external_function_type * {
      return nullptr;
    }
  };

  template<class T>
  concept bindings_concepts = requires(const T &t) {
    { t.get_callable_by_id(external_function_id{}) } -> std::convertible_to<external_function_type *>;
  };

  template<bindings_concepts bindings_t = empty_bindings>
  class context : public context_base {
  public:
    explicit context(std::span<const std::byte> bytes, bindings_t binds = {},
                     std::size_t stack_slots = default_stack_slots)
      : context_base(bytes, stack_slots), m_bindings(std::move(binds)) {
    }

    /**
     * Runs the script function with arguments given by value (structs are passed field by field).
     * Not re-entrant: do not call it from a bound function.
     */
    template<class Signature, class ...Args>
    auto call(function_handle<Signature> func, Args &&...args) -> typename function_traits<Signature>::return_type {
      using traits = function_traits<Signature>;
      using args_tuple = typename traits::args_tuple;
      using return_type = typename traits::return_type;

      format_static_assert<traits::args_count == sizeof...(args), [] {
        return korka::format("This function requires ~ arguments, got ~", traits::args_count, sizeof...(args));
      }>();

      constexpr std::size_t arg_slots = tuple_slots_v<args_tuple>;
      constexpr std::size_t ret_slots = slot_count_v<return_type>;

      // Leaves the context as it was, also when the script throws
      struct restore {
        context &ctx;
        value *sp;
        value *base;
        std::size_t frames;

        ~restore() {
          ctx.reset(sp, base, frames);
        }
      } guard{*this, m_sp, m_base, m_frames.size()};

      if (m_stack.size() - stack_depth() < arg_slots + operand_reserve) {
        throw stack_overflow{};
      }

      value *out = m_sp;
      [&]<std::size_t ...I>(std::index_sequence<I...>) {
        auto given = std::forward_as_tuple(std::forward<Args>(args)...);
        ([&] {
          using param_t = std::tuple_element_t<I, args_tuple>;
          write_slots<param_t>(std::get<I>(given), out);
          out += slot_count_v<param_t>;
        }(), ...);
      }(std::make_index_sequence<sizeof...(Args)>());

      // Returning from this frame ends `run`
      m_frames.push_back({nullptr, m_base});
      m_base = m_sp;
      m_sp = out;

      run(func.start_pos);

      if constexpr (not std::is_void_v<return_type>) {
        return read_slots<return_type>(m_sp - ret_slots);
      }
    }

  protected:
    bindings_t m_bindings;

    // Reads an operand and advances `ip`. memcpy, because operands are not aligned.
    template<class T>
    [[gnu::always_inline]] static inline auto read(const std::byte *&ip) -> T {
      T v;
      std::memcpy(&v, ip, sizeof(T));
      ip += sizeof(T);
      return v;
    }

    [[gnu::always_inline]] static inline auto as_i64(const value &v) -> std::int64_t {
      return std::bit_cast<std::int64_t>(v);
    }

    [[gnu::always_inline]] static inline auto from_i64(std::int64_t v) -> value {
      return std::bit_cast<value>(v);
    }

    // Wrap-around arithmetic: overflow in a script must not be undefined behaviour of the VM.
    [[gnu::always_inline]] static inline auto wrapping(std::int64_t a, std::int64_t b, auto op) -> std::int64_t {
      return static_cast<std::int64_t>(op(static_cast<std::uint64_t>(a), static_cast<std::uint64_t>(b)));
    }

    void run(std::size_t start_pos) {
      const std::byte *const code = m_code.data();
      const std::byte *ip = code + start_pos;
      const std::byte *instr = ip; // first byte of the current instruction
      value *sp = m_sp;
      value *base = m_base;
      const std::size_t stack_size = m_stack.size();
      const value *const stack_begin = m_stack.data();

      static const void *const dispatch[] = {
        &&op_lload,
        &&op_lsave,
        &&op_lloadn,
        &&op_lsaven,
        &&op_drop,
        &&op_pick,
        &&op_i64_const,
        &&op_i64_add,
        &&op_i64_sub,
        &&op_i64_mul,
        &&op_i64_div,
        &&op_i64_mod,
        &&op_i64_neg,
        &&op_i64_not,
        &&op_i64_eq,
        &&op_i64_ne,
        &&op_i64_lt,
        &&op_i64_le,
        &&op_i64_gt,
        &&op_i64_ge,
        &&op_jmp,
        &&op_jmpz,
        &&op_jmpnz,
        &&op_enter,
        &&op_call,
        &&op_ret,
        &&op_trap,
      };
      static_assert(std::size(dispatch) == op_code_count);

#define KORKA_NEXT() do { instr = ip; goto *dispatch[read<std::uint8_t>(ip)]; } while (false)
#define KORKA_BINARY(expression) \
      { \
        const std::int64_t b = as_i64(sp[-1]); \
        const std::int64_t a = as_i64(sp[-2]); \
        --sp; \
        sp[-1] = from_i64(expression); \
        KORKA_NEXT(); \
      }

      KORKA_NEXT();

      op_lload:
      {
        const auto slot = read<slot_t>(ip);
        *sp++ = base[slot];
        KORKA_NEXT();
      }

      op_lsave:
      {
        const auto slot = read<slot_t>(ip);
        base[slot] = *--sp;
        KORKA_NEXT();
      }

      op_lloadn:
      {
        const auto slot = read<slot_t>(ip);
        const auto n = read<slot_t>(ip);
        std::memcpy(sp, base + slot, n * sizeof(value));
        sp += n;
        KORKA_NEXT();
      }

      op_lsaven:
      {
        const auto slot = read<slot_t>(ip);
        const auto n = read<slot_t>(ip);
        sp -= n;
        std::memcpy(base + slot, sp, n * sizeof(value));
        KORKA_NEXT();
      }

      op_drop:
      {
        sp -= read<slot_t>(ip);
        KORKA_NEXT();
      }

      op_pick:
      {
        const auto total = read<slot_t>(ip);
        const auto offset = read<slot_t>(ip);
        const auto n = read<slot_t>(ip);
        value *first = sp - total;
        std::memmove(first, first + offset, n * sizeof(value));
        sp = first + n;
        KORKA_NEXT();
      }

      op_i64_const:
      {
        *sp++ = from_i64(read<std::int64_t>(ip));
        KORKA_NEXT();
      }

      op_i64_add: KORKA_BINARY(wrapping(a, b, std::plus<>{}))
      op_i64_sub: KORKA_BINARY(wrapping(a, b, std::minus<>{}))
      op_i64_mul: KORKA_BINARY(wrapping(a, b, std::multiplies<>{}))
      op_i64_div: KORKA_BINARY(a / b)
      op_i64_mod: KORKA_BINARY(a % b)

      op_i64_neg:
      {
        sp[-1] = from_i64(wrapping(0, as_i64(sp[-1]), std::minus<>{}));
        KORKA_NEXT();
      }

      op_i64_not:
      {
        sp[-1] = from_i64(as_i64(sp[-1]) == 0);
        KORKA_NEXT();
      }

      op_i64_eq: KORKA_BINARY(a == b)
      op_i64_ne: KORKA_BINARY(a != b)
      op_i64_lt: KORKA_BINARY(a < b)
      op_i64_le: KORKA_BINARY(a <= b)
      op_i64_gt: KORKA_BINARY(a > b)
      op_i64_ge: KORKA_BINARY(a >= b)

      op_jmp:
      {
        ip = instr + read<jump_offset>(ip);
        KORKA_NEXT();
      }

      op_jmpz:
      {
        const auto offset = read<jump_offset>(ip);
        if (as_i64(*--sp) == 0) ip = instr + offset;
        KORKA_NEXT();
      }

      op_jmpnz:
      {
        const auto offset = read<jump_offset>(ip);
        if (as_i64(*--sp) != 0) ip = instr + offset;
        KORKA_NEXT();
      }

      op_enter:
      {
        const auto local_slots = read<slot_t>(ip);
        const auto used = static_cast<std::size_t>(base - stack_begin);
        if (__builtin_expect(used + local_slots + operand_reserve > stack_size or m_frames.size() >= stack_size, 0)) {
          m_sp = sp;
          m_base = base;
          throw stack_overflow{};
        }
        // `sp - base` argument slots are already in place
        value *const frame_top = base + local_slots;
        std::fill(sp, frame_top, value{});
        sp = frame_top;
        KORKA_NEXT();
      }

      op_call:
      {
        const auto address = read<address_t>(ip);
        const auto arg_slots = read<slot_t>(ip);
        m_frames.push_back({ip, base});
        base = sp - arg_slots;
        ip = code + address;
        KORKA_NEXT();
      }

      op_ret:
      {
        const auto n = read<slot_t>(ip);
        if (n == 1) {
          *base = sp[-1];
        } else if (n > 1) {
          std::memmove(base, sp - n, n * sizeof(value));
        }
        sp = base + n;

        const frame caller = m_frames.back();
        m_frames.pop_back();
        base = caller.caller_base;
        ip = caller.return_ip;
        if (ip == nullptr) {
          m_sp = sp;
          m_base = base;
          return;
        }
        KORKA_NEXT();
      }

      op_trap:
      {
        const auto id = read<external_function_id>(ip);
        m_sp = sp;
        m_base = base;
        (*m_bindings.get_callable_by_id(id))(*this);
        sp = m_sp;
        KORKA_NEXT();
      }

#undef KORKA_NEXT
#undef KORKA_BINARY
    }
  };
} // korka::vm
