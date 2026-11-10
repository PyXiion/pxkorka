#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <vector>
#include "korka/shared/error.hpp"
#include "korka/utils/byte_writer.hpp"
#include "op_codes.hpp"

namespace korka::vm {
  class bytecode_builder {
  public:
    struct label {
      std::size_t id;
    };

    constexpr auto make_label() -> label {
      m_label_pos.push_back(unbound);
      return {m_label_pos.size() - 1};
    }

    /**
     * Saves the label on the next byte
     */
    constexpr auto bind_label(const label &l) -> void {
      m_label_pos[l.id] = m_data.size();
    }

    /**
     * Position of a bound label, `unbound` otherwise.
     */
    constexpr auto label_position(const label &l) const -> std::size_t {
      return m_label_pos[l.id];
    }

    constexpr auto position() const -> std::size_t {
      return m_data.size();
    }

    // --- ops ---
    constexpr auto emit_op(op_code code) -> std::size_t {
      return m_data.write(static_cast<std::uint8_t>(code));
    }

    constexpr auto emit_load_local(slot_t slot) -> void {
      emit_op(op_code::lload);
      m_data.write(slot);
    }

    constexpr auto emit_save_local(slot_t slot) -> void {
      emit_op(op_code::lsave);
      m_data.write(slot);
    }

    constexpr auto emit_load_locals(slot_t slot, slot_t count) -> void {
      emit_op(op_code::lloadn);
      m_data.write(slot);
      m_data.write(count);
    }

    constexpr auto emit_save_locals(slot_t slot, slot_t count) -> void {
      emit_op(op_code::lsaven);
      m_data.write(slot);
      m_data.write(count);
    }

    constexpr auto emit_drop(slot_t count) -> void {
      emit_op(op_code::drop);
      m_data.write(count);
    }

    constexpr auto emit_pick(slot_t total, slot_t offset, slot_t count) -> void {
      emit_op(op_code::pick);
      m_data.write(total);
      m_data.write(offset);
      m_data.write(count);
    }

    constexpr auto emit_i64_const(std::int64_t value) -> void {
      emit_op(op_code::i64_const);
      m_data.write(value);
    }

    // --- JUMPS ---
    constexpr auto emit_jmp(const label &target) -> void {
      record_patch(op_code::jmp, target, patch_kind::jump);
    }

    constexpr auto emit_jmp_if_zero(const label &target) -> void {
      record_patch(op_code::jmpz, target, patch_kind::jump);
    }

    constexpr auto emit_jmp_if_not_zero(const label &target) -> void {
      record_patch(op_code::jmpnz, target, patch_kind::jump);
    }

    // --- FUNCTIONS ---
    /**
     * Emits `enter` with a zero slot count, returns the position to give to `patch_enter`.
     */
    constexpr auto emit_enter() -> std::size_t {
      emit_op(op_code::enter);
      return m_data.write(slot_t{});
    }

    constexpr auto patch_enter(std::size_t operand_pos, slot_t local_slots) -> void {
      m_data.patch(operand_pos, local_slots);
    }

    constexpr auto emit_call(const label &function, slot_t arg_slots) -> void {
      record_patch(op_code::call, function, patch_kind::address);
      m_data.write(arg_slots);
    }

    constexpr auto emit_ret(slot_t ret_slots) -> void {
      emit_op(op_code::ret);
      m_data.write(ret_slots);
    }

    constexpr auto emit_trap(external_function_id function_id) -> void {
      emit_op(op_code::trap);
      m_data.write(function_id);
    }

    /**
     * Resolves jumps and calls. Fails if one of them targets a label that was never bound.
     */
    constexpr auto build() -> std::expected<std::vector<std::byte>, error_t> {
      for (const auto &p: m_patches) {
        const auto target = m_label_pos[p.target.id];
        if (target == unbound) {
          return std::unexpected{error::other_compiler_error{"A jump or a call targets a label that was never bound"}};
        }

        if (p.kind == patch_kind::jump) {
          m_data.patch(p.instr_pos + op_code_size, static_cast<jump_offset>(target - p.instr_pos));
        } else {
          m_data.patch(p.instr_pos + op_code_size, static_cast<address_t>(target));
        }
      }

      return m_data.data();
    }

    static constexpr std::size_t unbound = std::numeric_limits<std::size_t>::max();

  private:
    static constexpr std::size_t op_code_size = 1;

    enum class patch_kind { jump, address };

    struct pending_patch {
      std::size_t instr_pos;
      label target;
      patch_kind kind;
    };

    byte_writer m_data;
    std::vector<pending_patch> m_patches;
    std::vector<std::size_t> m_label_pos;

    // Emits `op` and a 4-byte operand that `build()` fills in.
    constexpr auto record_patch(op_code op, const label &target, patch_kind kind) -> void {
      const auto pos = emit_op(op);
      m_data.write(std::uint32_t{});
      m_patches.push_back({pos, target, kind});
    }
  };
}
