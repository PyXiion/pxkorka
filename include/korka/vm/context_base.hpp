#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include "korka/vm/op_codes.hpp"
#include "korka/vm/value.hpp"

namespace korka::vm {
  class context_base;

  // Signature of a C++ function wrapped for the `trap` instruction
  using external_function_type = void(context_base &context);

  // Allocator that leaves new elements uninitialised: the stack is large, a context should not pay
  // for touching all of it. Every slot is written before it is read (`enter` zero-fills the locals).
  template<class T>
  struct default_init_allocator : std::allocator<T> {
    template<class U>
    struct rebind {
      using other = default_init_allocator<U>;
    };

    using std::allocator<T>::allocator;

    template<class U>
    void construct(U *p) {
      ::new(static_cast<void *>(p)) U;
    }

    template<class U, class... Args>
    void construct(U *p, Args &&...args) {
      ::new(static_cast<void *>(p)) U(std::forward<Args>(args)...);
    }
  };

  struct stack_overflow : std::runtime_error {
    stack_overflow() : std::runtime_error("korka: VM stack overflow") {}
  };

  /**
   * Owns the VM state: one contiguous value stack allocated once, the stack pointer,
   * the base of the current frame, and the frame list.
   */
  class context_base {
  public:
    static constexpr std::size_t default_stack_slots = 65536;

    // `enter` keeps this many slots free above a frame for the operand stack. The depth of the
    // operand stack inside a single expression is not checked beyond that reserve.
    static constexpr std::size_t operand_reserve = 1024;

    explicit context_base(std::span<const std::byte> code, std::size_t stack_slots = default_stack_slots)
      : m_code(code), m_stack(stack_slots) {
      m_sp = m_base = m_stack.data();
      m_frames.reserve(256);
    }

    context_base(const context_base &) = delete;

    auto operator=(const context_base &) -> context_base & = delete;

    context_base(context_base &&) noexcept = default;

    auto operator=(context_base &&) noexcept -> context_base & = default;

    /**
     * Number of values on the stack. It is back at its starting value after every `call`.
     */
    auto stack_depth() const -> std::size_t {
      return static_cast<std::size_t>(m_sp - m_stack.data());
    }

    // --- Access for bound functions ---

    /**
     * The top `n` slots, first one lowest.
     */
    auto top_slots(std::size_t n) const -> const value * {
      return m_sp - n;
    }

    auto drop_slots(std::size_t n) -> void {
      m_sp -= n;
    }

    /**
     * Pushes `n` slots, returns the first of them.
     */
    auto push_slots(std::size_t n) -> value * {
      value *first = m_sp;
      m_sp += n;
      return first;
    }

  protected:
    struct frame {
      const std::byte *return_ip; // nullptr: return to the C++ caller of `call`
      value *caller_base;
    };

    // Drops the frames above `frames`, puts the stack pointer and the frame base back
    auto reset(value *sp, value *base, std::size_t frames) -> void {
      m_frames.erase(m_frames.begin() + static_cast<std::ptrdiff_t>(frames), m_frames.end());
      m_sp = sp;
      m_base = base;
    }

    std::span<const std::byte> m_code;
    std::vector<value, default_init_allocator<value>> m_stack;
    std::vector<frame> m_frames;
    value *m_sp{};
    value *m_base{};
  };
}
