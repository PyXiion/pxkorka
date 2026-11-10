#pragma once

#include <cstddef>
#include <cstdint>

namespace korka::vm {
  using slot_t = std::uint16_t; // slot index or slot count
  using jump_offset = std::int32_t; // relative to the first byte of the jump instruction
  using address_t = std::uint32_t; // absolute byte offset into the bytecode
  using external_function_id = std::uint16_t;

  /**
   * The VM works on one contiguous stack of untyped 8-byte slots.
   * A function's locals (arguments first) sit at `base + slot`, the operand stack grows above them.
   * A struct value is just `n` consecutive slots, first field in the lowest slot.
   *
   * Operands follow the opcode byte directly, in host byte order, unaligned.
   * Stack effects are written as `before -> after`, the rightmost value is the top of the stack.
   */
  enum class op_code : std::uint8_t {
    // --- Locals ---
    // Pushes the local slot.
    // <op><slot_t slot>              ... -> ... v
    lload,

    // Pops a value into the local slot.
    // <op><slot_t slot>              ... v -> ...
    lsave,

    // Pushes `n` consecutive local slots, order preserved.
    // <op><slot_t slot><slot_t n>    ... -> ... v0 .. v(n-1)
    lloadn,

    // Pops `n` values into `n` consecutive local slots, order preserved.
    // <op><slot_t slot><slot_t n>    ... v0 .. v(n-1) -> ...
    lsaven,

    // Discards `n` slots.
    // <op><slot_t n>                 ... v0 .. v(n-1) -> ...
    drop,

    // Of the top `total` slots keeps `n` slots starting at `offset` (a member of a temporary).
    // <op><slot_t total><slot_t offset><slot_t n>
    //                                ... v0 .. v(total-1) -> ... v(offset) .. v(offset+n-1)
    pick,

    // --- Arithmetic ---
    // Pushes a constant.
    // <op><i64:8>                    ... -> ... c
    i64_const,

    // Binary operations: B = pop(), A = pop(), push(A <op> B). Add, sub, mul and neg wrap around.
    // <op>                           ... A B -> ... C
    i64_add,
    i64_sub,
    i64_mul,
    i64_div,
    i64_mod,

    // <op>                           ... A -> ... -A
    i64_neg,
    // <op>                           ... A -> ... (A == 0)
    i64_not,

    // Comparisons: B = pop(), A = pop(), push(A <op> B ? 1 : 0).
    // <op>                           ... A B -> ... 0|1
    i64_eq,
    i64_ne,
    i64_lt,
    i64_le,
    i64_gt,
    i64_ge,

    // --- Control flow ---
    // Jumps unconditionally.
    // <op><jump_offset>
    jmp,

    // Pops a value, jumps if it is zero.
    // <op><jump_offset>              ... v -> ...
    jmpz,

    // Pops a value, jumps if it is not zero.
    // <op><jump_offset>              ... v -> ...
    jmpnz,

    // Function prologue. On entry the arguments are the first locals (`sp - base` slots).
    // Reserves and zero-fills the remaining of `local_slots` (arguments included) locals.
    // Throws `stack_overflow` when the stack cannot hold the frame.
    // <op><slot_t local_slots>
    enter,

    // Calls the function at `address`. The top `arg_slots` slots become its first locals,
    // nothing is copied. The callee leaves its result where the arguments were.
    // <op><address_t address><slot_t arg_slots>
    //                                ... args -> ... result
    call,

    // Returns from the function: moves the top `ret_slots` slots to the frame base.
    // <op><slot_t ret_slots>         ... locals .. result -> result
    ret,

    // Calls a bound C++ function; it pops its arguments and pushes its result itself.
    // <op><external_function_id id>  ... args -> ... result
    trap,

    count_ // not an instruction
  };

  constexpr std::size_t op_code_count = static_cast<std::size_t>(op_code::count_);
}
