#include <catch_amalgamated.hpp>
#include "korka/vm/bytecode_builder.hpp"
#include "korka/utils/byte_writer.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/vm/options.hpp"

using namespace korka;
using namespace korka::vm;

TEST_CASE("Building byte codes", "[bytecode_builder]") {
  korka::vm::bytecode_builder builder{};

  builder.emit_add(0, 1, 2);

  auto bytes = builder.build();

  korka::byte_writer expected_builder{};
  expected_builder.write<korka::vm::op_code_size>(static_cast<int>(korka::vm::op_code::add));
  expected_builder.write_many(korka::vm::reg_id_t{0}, korka::vm::reg_id_t{1}, korka::vm::reg_id_t{2});
  auto &expected_bytes = expected_builder.data();

  REQUIRE(bytes == expected_bytes);
}

TEST_CASE("Arithmetic instructions", "[bytecode_builder]") {
  korka::vm::bytecode_builder b{};

  b.emit_add(0, 1, 2);
  b.emit_sub(3, 4, 5);
  b.emit_mul(6, 7, 8);
  b.emit_div(9, 10, 11);

  auto bytes = b.build();

  korka::byte_writer expected{};
  expected.write<korka::vm::op_code_size>(int(korka::vm::op_code::add));
  expected.write_many(reg_id_t{0}, reg_id_t{1}, reg_id_t{2});

  expected.write<korka::vm::op_code_size>(int(korka::vm::op_code::sub));
  expected.write_many(reg_id_t{3}, reg_id_t{4}, reg_id_t{5});

  expected.write<korka::vm::op_code_size>(int(korka::vm::op_code::mul));
  expected.write_many(reg_id_t{6}, reg_id_t{7}, reg_id_t{8});

  expected.write<korka::vm::op_code_size>(int(korka::vm::op_code::div));
  expected.write_many(reg_id_t{9}, reg_id_t{10}, reg_id_t{11});

  REQUIRE(bytes == expected.data());
}

TEST_CASE("Load immediate", "[bytecode_builder]") {
  korka::vm::bytecode_builder b{};

  b.emit_load_imm(3, stack_value_t{42});
  auto bytes = b.build();

  korka::byte_writer expected{};
  expected.write<korka::vm::op_code_size>(int(korka::vm::op_code::load_imm));
  expected.write_many(reg_id_t{3}, stack_value_t{42});

  REQUIRE(bytes == expected.data());
}

TEST_CASE("Compare instructions", "[bytecode_builder]") {
  korka::vm::bytecode_builder b{};

  b.emit_cmp_lt(0, 1, 2);
  b.emit_cmp_gt(3, 4, 5);
  b.emit_cmp_eq(6, 7, 8);

  auto bytes = b.build();

  korka::byte_writer expected{};
  expected.write<op_code_size>(int(op_code::cmp_lt));
  expected.write_many(reg_id_t{0}, reg_id_t{1}, reg_id_t{2});

  expected.write<op_code_size>(int(op_code::cmp_gt));
  expected.write_many(reg_id_t{3}, reg_id_t{4}, reg_id_t{5});

  expected.write<op_code_size>(int(op_code::cmp_eq));
  expected.write_many(reg_id_t{6}, reg_id_t{7}, reg_id_t{8});

  REQUIRE(bytes == expected.data());
}

TEST_CASE("Unconditional jump forward", "[bytecode_builder]") {
  using namespace korka::vm;

  bytecode_builder b{};
  auto target = b.make_label();

  b.emit_jmp(target);
  b.emit_add(0, 1, 2);
  b.bind(target);
  b.emit_add(3, 4, 5);

  auto bytes = b.build();

  byte_writer expected{};

  expected.write<op_code_size>(int(op_code::jmp));
  std::int64_t offset =
    op_code_size + sizeof(std::int64_t) + // jmp size
    op_code_size + sizeof(reg_id_t) * 3;  // add size
  expected.write_many(offset);

  expected.write<op_code_size>(int(op_code::add));
  expected.write_many(reg_id_t{0}, reg_id_t{1}, reg_id_t{2});

  expected.write<op_code_size>(int(op_code::add));
  expected.write_many(reg_id_t{3}, reg_id_t{4}, reg_id_t{5});

  REQUIRE(bytes == expected.data());
}

TEST_CASE("Conditional jump", "[bytecode_builder]") {
  using namespace korka::vm;

  bytecode_builder b{};

  auto target = b.make_label();

  b.emit_jmp_if(target, reg_id_t{7});
  b.emit_add(0, 1, 2);
  b.bind(target);
  b.emit_add(3, 4, 5);

  auto bytes = b.build();

  byte_writer expected{};

  expected.write<op_code_size>(int(op_code::jmp_if));
  std::int64_t offset =
    op_code_size + sizeof(std::int64_t) + sizeof(reg_id_t) + // jmp size
    op_code_size + sizeof(reg_id_t) * 3; // add size
  expected.write_many(offset);
  expected.write_many(reg_id_t{7});

  expected.write<op_code_size>(int(op_code::add));
  expected.write_many(reg_id_t{0}, reg_id_t{1}, reg_id_t{2});

  expected.write<op_code_size>(int(op_code::add));
  expected.write_many(reg_id_t{3}, reg_id_t{4}, reg_id_t{5});

  REQUIRE(bytes == expected.data());
}

TEST_CASE("Backward jump (loop)", "[bytecode_builder]") {
  using namespace korka::vm;

  bytecode_builder b{};

  auto loop = b.make_label();
  b.bind(loop);

  b.emit_add(0, 1, 2);
  b.emit_jmp(loop);

  auto bytes = b.build();

  byte_writer expected{};

  expected.write<op_code_size>(int(op_code::add));
  expected.write_many(reg_id_t{0}, reg_id_t{1}, reg_id_t{2});

  expected.write<op_code_size>(int(op_code::jmp));
  std::int64_t offset =
    -static_cast<std::int64_t>(
      op_code_size + sizeof(reg_id_t) * 3);
  expected.write_many(offset);

  REQUIRE(bytes == expected.data());
}
