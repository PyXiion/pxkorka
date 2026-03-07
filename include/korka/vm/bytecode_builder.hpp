#include "korka/utils/byte_writer.hpp"
#include "korka/utils/utils.hpp"
#include "korka/shared/types.hpp"
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

    /**
     * Saves the label on the next byte
     */
    constexpr auto bind_label(const label &l) -> auto {
      m_label_pos.emplace_back(l, m_data.data().size());
    }

    constexpr auto resolve_label(const label &label_) -> std::optional<int> {
      for (auto &&[l, i]: m_label_pos) {
        if (l.id == label_.id)
          return i;
      }
      return std::nullopt;
    }

    // ops

    constexpr auto emit_op(op_code code) -> std::size_t {
      return m_last_op_pos = m_data.write<op_code_size>(static_cast<int>(code));
    }

    constexpr auto emit_load_local(local_index_t index) {
      emit_op(op_code::lload);
      m_data.write_many(index);
    }

    constexpr auto emit_save_local(local_index_t index) {
      emit_op(op_code::lsave);
      m_data.write_many(index);
    }

    template<korka::type Type>
    constexpr auto emit_const(const type_to_cpp_t<Type> &value) {
      emit_op(get_const_op_by_type<Type>());
      m_data.write_many(value);
    }

    // --- JUMPS ---
    constexpr auto emit_jmp(const label &target) {
      record_jump(op_code::jmp, target);
    }
    constexpr auto emit_jmp_if_zero(const label &target) {
      record_jump(op_code::jmpz, target);
    }
//
//    constexpr auto emit_jmp_if(const label &target, reg_id_t cond) {
//      record_jump(op_code::jmp_if, target, cond);
//    }

    constexpr auto build() -> std::vector<std::byte> {
      auto data = m_data.data();
      for (auto &&j: m_jumps) {
        auto label_pos = resolve_label(j.target);
        if (not label_pos) {
          std::abort();
        }
        int target_pc = *label_pos;
        jump_offset offset = target_pc - j.instr_index;

        std::ranges::copy(
          std::bit_cast<std::array<std::byte, sizeof(offset)>>(offset),
          std::begin(data) + j.instr_index + op_code_size);
      }

      return data;
    }

  private:
    struct pending_jump {
      jump_offset instr_index;
      label target;
    };

    byte_writer m_data;
    reg_id_t m_next_reg{};
    int next_label{};
    std::size_t m_last_op_pos{};

    std::vector<pending_jump> m_jumps;
    std::vector<std::pair<label, int>> m_label_pos;

    constexpr auto
    record_jump(op_code op, const label &label_) -> void {
      auto index = emit_op(op);
      m_data.write_many(jump_offset{});
      m_jumps.emplace_back(index, label_);
    }
  };

  namespace tests {
//    constexpr auto builder = []() constexpr {
//      constexpr static auto get_bytes = []() constexpr {
//        bytecode_builder b;
//        b.emit_add(0, 1, 2);
//        return b.build();
//      };
//      constexpr static auto bytes = to_array<get_bytes>();
//      return bytes;
//    };
//
//    constexpr auto bytes = builder();
//    static_assert(bytes == std::array<std::byte, 4>{
//      static_cast<std::byte>(op_code::add),
//      static_cast<std::byte>(0),
//      static_cast<std::byte>(1),
//      static_cast<std::byte>(2)
//    });
  }
}