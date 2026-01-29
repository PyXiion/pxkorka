#pragma once

namespace korka::vm {
  enum class op_code {
    load_imm, // rX = imm
    load_arg, // rX = arg[i]

    add,   // rC = rA + rB
    sub,   // rC = rA - rB
    mul,   // rC = rA * rB
    div,   // rC = rA / rB

    cmp_eq, // rC = rA == rB
    cmp_lt, // rC = rA < rB
    cmp_gt, // rC = rA > rB

    jmp,    // pc += offset
    jmp_if, // if (rX) pc += offset

    call,
    ret
  };

  constexpr int op_code_size = 1;
}