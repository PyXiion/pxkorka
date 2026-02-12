#include "korka/utils/byte_writer.hpp"
#include "korka/utils/utils.hpp"
#include "op_codes.hpp"
#include "options.hpp"
#include <cstdlib>
#include <optional>

namespace korka::vm {
  class bytecode_builder {
  public:
    struct label {
      int id;
    };

  public:
    constexpr auto new_reg() -> reg_id_t {
      return m_next_reg++;
    }

    constexpr auto make_label() -> label {
      return {next_label++};
    }

    constexpr auto bind(const label &l) -> auto {
      m_label_pos.emplace_back(l, m_data.data().size());
    }

    constexpr auto emit_load_imm(reg_id_t dst, stack_value_t imm) {
      emit_op(op_code::load_imm);
      m_data.write_many(dst, imm);
    }

    constexpr auto emit_add(reg_id_t dst, reg_id_t a, reg_id_t b) {
      emit_op(op_code::add);
      m_data.write_many(dst, a, b);
    }

    constexpr auto emit_sub(reg_id_t dst, reg_id_t a, reg_id_t b) {
      emit_op(op_code::sub);
      m_data.write_many(dst, a, b);
    }

    constexpr auto emit_mul(reg_id_t dst, reg_id_t a, reg_id_t b) {
      emit_op(op_code::mul);
      m_data.write_many(dst, a, b);
    }

    constexpr auto emit_div(reg_id_t dst, reg_id_t a, reg_id_t b) {
      emit_op(op_code::div);
      m_data.write_many(dst, a, b);
    }

    constexpr auto emit_cmp_lt(reg_id_t dst, reg_id_t a, reg_id_t b) {
      emit_op(op_code::cmp_lt);
      m_data.write_many(dst, a, b);
    }

    constexpr auto emit_cmp_gt(reg_id_t dst, reg_id_t a, reg_id_t b) {
      emit_op(op_code::cmp_gt);
      m_data.write_many(dst, a, b);
    }

    constexpr auto emit_cmp_eq(reg_id_t dst, reg_id_t a, reg_id_t b) {
      emit_op(op_code::cmp_eq);
      m_data.write_many(dst, a, b);
    }

    // --- JUMPS ---
    constexpr auto emit_jmp(const label &target) {
      record_jump(op_code::jmp, target);
    }

    constexpr auto emit_jmp_if(const label &target, reg_id_t cond) {
      record_jump(op_code::jmp_if, target, cond);
    }

    constexpr auto build() -> std::vector<std::byte> {
      auto data = m_data.data();
      for (auto &&j: m_jumps) {
        auto label_pos = get_label_pos(j.target);
        if (not label_pos) {
          std::abort();
        }
        int target_pc = *label_pos;
        std::int64_t offset = target_pc - j.instr_index;
        std::ranges::copy(
          std::as_bytes(std::span{&offset, 1}),
          std::begin(data) + j.instr_index + op_code_size);
      }

      return data;
    }

  private:
    struct pending_jump {
      int instr_index;
      label target;
    };

    byte_writer m_data;
    reg_id_t m_next_reg{};
    int next_label{};
    std::size_t m_last_op_pos{};

    std::vector<pending_jump> m_jumps;
    std::vector<std::pair<label, int>> m_label_pos;

    constexpr auto get_label_pos(const label &label_) -> std::optional<int> {
      for (auto &&[l, i]: m_label_pos) {
        if (l.id == label_.id)
          return i;
      }
      return std::nullopt;
    }

    constexpr auto emit_op(op_code code) -> std::size_t {
      return m_last_op_pos = m_data.write<op_code_size>(static_cast<int>(code));
    }

    constexpr auto
    record_jump(op_code op, const label &label_, std::optional<reg_id_t> condition = std::nullopt) -> void {
      auto index = emit_op(op);
      m_data.write_many(std::int64_t{});
      if (condition)
        m_data.write_many(*condition);
      m_jumps.emplace_back(index, label_);
    }
  };

  namespace tests {
    constexpr auto builder = []() constexpr {
      constexpr static auto get_bytes = []() constexpr {
        bytecode_builder b;
        b.emit_add(0, 1, 2);
        return b.build();
      };
      constexpr static auto bytes = to_array<get_bytes>();
      return bytes;
    };

    constexpr auto bytes = builder();
    static_assert(bytes == std::array<std::byte, 4>{
      static_cast<std::byte>(op_code::add),
      static_cast<std::byte>(0),
      static_cast<std::byte>(1),
      static_cast<std::byte>(2)
    });
  }
}