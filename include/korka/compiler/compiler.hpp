#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include "korka/compiler/binding.hpp"
#include "korka/compiler/info.hpp"
#include "korka/compiler/parser.hpp"
#include "korka/compiler/result.hpp"
#include "korka/compiler/symbols.hpp"
#include "korka/compiler/type_table.hpp"
#include "korka/shared/error.hpp"
#include "korka/shared/types.hpp"
#include "korka/vm/bytecode_builder.hpp"
#include "korka/vm/op_codes.hpp"

namespace korka {
  namespace detail {
    template<class T, class... Ts>
    inline constexpr bool is_one_of = (std::is_same_v<T, Ts> or ...);

    template<class N>
    inline constexpr bool is_expression_node = is_one_of<N,
      nodes::expr_literal, nodes::expr_var, nodes::expr_unary, nodes::expr_binary,
      nodes::expr_assign, nodes::expr_member, nodes::expr_call>;

    template<class N>
    inline constexpr bool is_statement_node = is_one_of<N,
      nodes::stmt_block, nodes::stmt_if, nodes::stmt_while, nodes::stmt_return,
      nodes::stmt_expr, nodes::decl_var>;
  }

  /**
   * Compiles the AST to bytecode in two passes: the first collects structs and function
   * signatures, the second compiles the function bodies.
   */
  template<class Bindings>
  class compiler {
    using result_t = std::expected<type_info, error_t>;
    using status_t = std::expected<void, error_t>;

    // A variable or a member chain rooted at a variable: where it lives in the frame
    struct place {
      std::size_t slot;
      type_info type;
    };

  public:
    constexpr compiler(std::span<const nodes::node> nodes, nodes::index_t root_node, const Bindings &bindings)
      : m_nodes(nodes), m_root_node(root_node), m_bindings(bindings) {
    }

    constexpr auto compile() -> std::expected<compilation_result, error_t> {
      const auto *program = std::get_if<nodes::decl_program>(&m_nodes[m_root_node].data);
      if (not program) {
        return std::unexpected{error::other_compiler_error{"The root node is not a program"}};
      }

      // Pass 1: structs in source order (bound ones first), then function signatures
      for (auto &info: m_bindings.exposed_structs()) {
        const auto name = info.name;
        if (not m_types.add(std::move(info))) {
          return std::unexpected{error::redeclaration{.identifier = name}};
        }
      }

      for (auto item: nodes::get_list_view(m_nodes, program->external_declarations_head)) {
        if (const auto *decl = std::get_if<nodes::decl_struct>(&m_nodes[item].data)) {
          if (auto ok = declare_struct(*decl); not ok) return std::unexpected{ok.error()};
        }
      }

      for (auto item: nodes::get_list_view(m_nodes, program->external_declarations_head)) {
        if (const auto *decl = std::get_if<nodes::decl_function>(&m_nodes[item].data)) {
          if (auto ok = declare_function(*decl); not ok) return std::unexpected{ok.error()};
        }
      }

      // Pass 2: bodies
      for (auto item: nodes::get_list_view(m_nodes, program->external_declarations_head)) {
        if (const auto *decl = std::get_if<nodes::decl_function>(&m_nodes[item].data)) {
          if (auto ok = compile_function(*decl); not ok) return std::unexpected{ok.error()};
        }
      }

      auto bytes = m_builder.build();
      if (not bytes) return std::unexpected{bytes.error()};

      return compilation_result{
        std::move(*bytes),
        std::move(m_symbols.functions)
      };
    }

  private:
    std::span<const nodes::node> m_nodes;
    nodes::index_t m_root_node;
    const Bindings &m_bindings;

    type_table m_types;
    symbol_table m_symbols;
    vm::bytecode_builder m_builder;

    // The function being compiled
    function_info m_function;
    std::size_t m_loop_depth{};

    // ---------------------------------------------------------------------------------------
    // Pass 1
    // ---------------------------------------------------------------------------------------

    constexpr auto declare_struct(const nodes::decl_struct &decl) -> status_t {
      if (m_types.contains_struct(decl.name)) {
        return std::unexpected{error::redeclaration{.identifier = decl.name}};
      }

      struct_info info{.name = decl.name};
      std::size_t offset = 0;
      for (auto field_idx: nodes::get_list_view(m_nodes, decl.fields_head)) {
        const auto &field = std::get<nodes::decl_var>(m_nodes[field_idx].data);

        auto type = m_types.lookup(field.type_name);
        if (not type) return std::unexpected{type.error()};
        if (type->is_void()) {
          return std::unexpected{error::other_compiler_error{"A struct field cannot be void"}};
        }

        for (const auto &previous: info.fields) {
          if (previous.name == field.var_name) {
            return std::unexpected{error::redeclaration{.identifier = field.var_name}};
          }
        }

        info.fields.push_back({field.var_name, *type, offset});
        offset += m_types.slot_count(*type);
      }

      if (offset > symbol_table::max_slots) {
        return std::unexpected{error::other_compiler_error{"A struct is too large"}};
      }
      info.slot_count = offset;
      m_types.add(std::move(info));
      return {};
    }

    constexpr auto declare_function(const nodes::decl_function &decl) -> status_t {
      auto return_type = m_types.lookup(decl.ret_type);
      if (not return_type) return std::unexpected{return_type.error()};

      function_info info{
        .name = decl.name,
        .return_type = *return_type,
        .return_slots = m_types.slot_count(*return_type)
      };

      for (auto param_idx: nodes::get_list_view(m_nodes, decl.params_head)) {
        const auto &param = std::get<nodes::decl_var>(m_nodes[param_idx].data);

        auto type = m_types.lookup(param.type_name);
        if (not type) return std::unexpected{type.error()};
        if (type->is_void()) {
          return std::unexpected{error::other_compiler_error{"A parameter cannot be void"}};
        }

        info.param_types.push_back(*type);
        info.param_slots += m_types.slot_count(*type);
      }

      if (info.param_slots > symbol_table::max_slots) {
        return std::unexpected{error::other_compiler_error{"A function has too many parameter slots"}};
      }

      if (m_bindings.contains(decl.name)) {
        return std::unexpected{error::redeclaration{.identifier = decl.name}};
      }

      info.label = m_builder.make_label();
      return m_symbols.declare_function(std::move(info));
    }

    // ---------------------------------------------------------------------------------------
    // Pass 2: functions and statements
    // ---------------------------------------------------------------------------------------

    constexpr auto compile_function(const nodes::decl_function &decl) -> status_t {
      m_function = *m_symbols.lookup_function(decl.name);

      m_builder.bind_label(m_function.label);
      m_symbols.functions[decl.name].start_pos = m_builder.position();

      m_symbols.begin_function();
      m_symbols.push_scope();
      m_loop_depth = 0;

      // Arguments are the first locals, already in place when the function starts
      std::size_t param_number = 0;
      for (auto param_idx: nodes::get_list_view(m_nodes, decl.params_head)) {
        const auto &param = std::get<nodes::decl_var>(m_nodes[param_idx].data);
        const auto &type = m_function.param_types[param_number++];
        if (auto ok = m_symbols.declare_var(param.var_name, type, m_types.slot_count(type)); not ok) {
          return std::unexpected{ok.error()};
        }
      }

      const auto enter_operand = m_builder.emit_enter();

      // The body shares the scope of the parameters
      const auto &body = std::get<nodes::stmt_block>(m_nodes[decl.body].data);
      for (auto stmt: nodes::get_list_view(m_nodes, body.children_head)) {
        if (auto ok = compile_stmt(stmt); not ok) return ok;
      }

      if (not definitely_returns(decl.body)) {
        if (not m_function.return_type.is_void()) {
          return std::unexpected{error::missing_return{.function_name = decl.name}};
        }
        m_builder.emit_ret(0);
      }

      m_builder.patch_enter(enter_operand, static_cast<vm::slot_t>(m_symbols.slot_count()));
      m_symbols.pop_scope();
      return {};
    }

    // A statement list definitely returns if its last statement is a `return`, a block that
    // definitely returns, or an `if`/`else` whose both branches do.
    constexpr auto definitely_returns(nodes::index_t idx) const -> bool {
      if (idx == nodes::empty_node) return false;

      const auto &data = m_nodes[idx].data;
      if (std::holds_alternative<nodes::stmt_return>(data)) return true;

      if (const auto *block = std::get_if<nodes::stmt_block>(&data)) {
        nodes::index_t last = nodes::empty_node;
        for (auto item: nodes::get_list_view(m_nodes, block->children_head)) last = item;
        return definitely_returns(last);
      }

      if (const auto *if_ = std::get_if<nodes::stmt_if>(&data)) {
        return if_->else_branch != nodes::empty_node
               and definitely_returns(if_->then_branch)
               and definitely_returns(if_->else_branch);
      }

      return false;
    }

    // `empty_node` is the empty statement
    constexpr auto compile_stmt(nodes::index_t idx) -> status_t {
      if (idx == nodes::empty_node) return {};

      return std::visit([&](const auto &node) -> status_t {
        return compile_stmt_node(node);
      }, m_nodes[idx].data);
    }

    // Every node kind that cannot be a statement is rejected here
    template<class N> requires (not detail::is_statement_node<N>)
    constexpr auto compile_stmt_node(const N &) -> status_t {
      return std::unexpected{error::other_compiler_error{"Expected a statement"}};
    }

    constexpr auto compile_stmt_node(const nodes::stmt_block &block) -> status_t {
      m_symbols.push_scope();
      for (auto stmt: nodes::get_list_view(m_nodes, block.children_head)) {
        if (auto ok = compile_stmt(stmt); not ok) return ok;
      }
      m_symbols.pop_scope();
      return {};
    }

    constexpr auto compile_stmt_node(const nodes::stmt_if &if_) -> status_t {
      if (auto ok = compile_condition(if_.condition); not ok) return ok;

      auto end_label = m_builder.make_label();

      if (if_.else_branch == nodes::empty_node) {
        m_builder.emit_jmp_if_zero(end_label);
        if (auto ok = compile_stmt(if_.then_branch); not ok) return ok;
      } else {
        auto else_label = m_builder.make_label();

        m_builder.emit_jmp_if_zero(else_label);
        if (auto ok = compile_stmt(if_.then_branch); not ok) return ok;
        m_builder.emit_jmp(end_label);

        m_builder.bind_label(else_label);
        if (auto ok = compile_stmt(if_.else_branch); not ok) return ok;
      }

      m_builder.bind_label(end_label);
      return {};
    }

    constexpr auto compile_stmt_node(const nodes::stmt_while &while_) -> status_t {
      auto start_label = m_builder.make_label();
      auto end_label = m_builder.make_label();

      m_builder.bind_label(start_label);
      if (auto ok = compile_condition(while_.condition); not ok) return ok;
      m_builder.emit_jmp_if_zero(end_label);

      ++m_loop_depth;
      auto body = compile_stmt(while_.body);
      --m_loop_depth;
      if (not body) return body;

      m_builder.emit_jmp(start_label);
      m_builder.bind_label(end_label);
      return {};
    }

    constexpr auto compile_stmt_node(const nodes::stmt_return &return_) -> status_t {
      type_info actual = type_info::void_();
      if (return_.expr != nodes::empty_node) {
        auto type = compile_expr(return_.expr);
        if (not type) return std::unexpected{type.error()};
        actual = *type;
      }

      if (actual != m_function.return_type) {
        return mismatch("return value", m_function.name, m_function.return_type, actual);
      }

      m_builder.emit_ret(static_cast<vm::slot_t>(m_function.return_slots));
      return {};
    }

    constexpr auto compile_stmt_node(const nodes::stmt_expr &stmt) -> status_t {
      // An assignment on its own does not need its value back
      if (const auto *assign = std::get_if<nodes::expr_assign>(&m_nodes[stmt.expr].data)) {
        auto type = compile_assign(*assign, false);
        if (not type) return std::unexpected{type.error()};
        return {};
      }

      auto type = compile_expr(stmt.expr);
      if (not type) return std::unexpected{type.error()};

      // The value is not used
      if (const auto slots = m_types.slot_count(*type); slots > 0) {
        m_builder.emit_drop(static_cast<vm::slot_t>(slots));
      }
      return {};
    }

    constexpr auto compile_stmt_node(const nodes::decl_var &var) -> status_t {
      auto type = m_types.lookup(var.type_name);
      if (not type) return std::unexpected{type.error()};
      if (type->is_void()) {
        return std::unexpected{error::other_compiler_error{"A variable cannot be void"}};
      }

      // The initialiser is compiled first, so it still sees an outer variable of the same name
      if (var.init_expr != nodes::empty_node) {
        auto init = compile_expr(var.init_expr);
        if (not init) return std::unexpected{init.error()};
        if (*init != *type) return mismatch("initializer", var.var_name, *type, *init);
      }

      const auto slots = m_types.slot_count(*type);
      auto declared = m_symbols.declare_var(var.var_name, *type, slots);
      if (not declared) return std::unexpected{declared.error()};

      if (var.init_expr != nodes::empty_node) {
        emit_store(declared->first_slot, slots);
      } else if (m_loop_depth > 0) {
        // The prologue zero-fills only once, a loop body needs a fresh variable every iteration
        for (std::size_t i = 0; i < slots; ++i) {
          m_builder.emit_i64_const(0);
          m_builder.emit_save_local(static_cast<vm::slot_t>(declared->first_slot + i));
        }
      }
      return {};
    }

    constexpr auto compile_condition(nodes::index_t idx) -> status_t {
      auto type = compile_expr(idx);
      if (not type) return std::unexpected{type.error()};
      if (not type->is_i64()) return mismatch("condition", "", type_info::i64(), *type);
      return {};
    }

    // ---------------------------------------------------------------------------------------
    // Pass 2: expressions. Each leaves its value on the stack and returns its type.
    // ---------------------------------------------------------------------------------------

    constexpr auto compile_expr(nodes::index_t idx) -> result_t {
      return std::visit([&](const auto &node) -> result_t {
        return compile_expr_node(node);
      }, m_nodes[idx].data);
    }

    // Every node kind that cannot be an expression is rejected here
    template<class N> requires (not detail::is_expression_node<N>)
    constexpr auto compile_expr_node(const N &) -> result_t {
      return std::unexpected{error::other_compiler_error{"Expected an expression"}};
    }

    constexpr auto compile_expr_node(const nodes::expr_literal &literal) -> result_t {
      if (const auto *value = std::get_if<std::int64_t>(&literal)) {
        m_builder.emit_i64_const(*value);
        return type_info::i64();
      }
      return std::unexpected{error::other_compiler_error{"Only integer literals are supported"}};
    }

    constexpr auto compile_expr_node(const nodes::expr_var &var) -> result_t {
      auto info = m_symbols.lookup_variable(var.name);
      if (not info) {
        return std::unexpected{error::undefined_symbol{.identifier = var.name}};
      }

      emit_load(info->first_slot, m_types.slot_count(info->type));
      return info->type;
    }

    constexpr auto compile_expr_node(const nodes::expr_unary &unary) -> result_t {
      if (auto ok = compile_operand(unary.child); not ok) return std::unexpected{ok.error()};

      switch (unary.op) {
        case nodes::unary_op::neg:
          m_builder.emit_op(vm::op_code::i64_neg);
          break;
        case nodes::unary_op::plus:
          break;
        case nodes::unary_op::not_:
          m_builder.emit_op(vm::op_code::i64_not);
          break;
      }
      return type_info::i64();
    }

    constexpr auto compile_expr_node(const nodes::expr_binary &binary) -> result_t {
      if (binary.op == nodes::binary_op::and_ or binary.op == nodes::binary_op::or_) {
        return compile_logical(binary);
      }

      if (auto ok = compile_operand(binary.left); not ok) return std::unexpected{ok.error()};
      if (auto ok = compile_operand(binary.right); not ok) return std::unexpected{ok.error()};

      m_builder.emit_op(arithmetic_op_code(binary.op));
      return type_info::i64();
    }

    // `and` / `or` evaluate the right side only if needed and yield 0 or 1
    constexpr auto compile_logical(const nodes::expr_binary &binary) -> result_t {
      const bool is_and = binary.op == nodes::binary_op::and_;

      auto short_circuit = m_builder.make_label();
      auto end = m_builder.make_label();

      for (auto operand: {binary.left, binary.right}) {
        if (auto ok = compile_operand(operand); not ok) return std::unexpected{ok.error()};
        if (is_and) m_builder.emit_jmp_if_zero(short_circuit);
        else m_builder.emit_jmp_if_not_zero(short_circuit);
      }

      m_builder.emit_i64_const(is_and ? 1 : 0);
      m_builder.emit_jmp(end);
      m_builder.bind_label(short_circuit);
      m_builder.emit_i64_const(is_and ? 0 : 1);
      m_builder.bind_label(end);
      return type_info::i64();
    }

    constexpr auto compile_expr_node(const nodes::expr_assign &assign) -> result_t {
      return compile_assign(assign, true);
    }

    // Stores the value into the target; with `keep_value` the assignment is also a value
    constexpr auto compile_assign(const nodes::expr_assign &assign, bool keep_value) -> result_t {
      auto target = resolve_place(assign.target);
      if (not target) return std::unexpected{target.error()};
      if (not *target) return std::unexpected{error::not_assignable{}};
      const place destination = **target;

      auto value = compile_expr(assign.value);
      if (not value) return value;
      if (*value != destination.type) return mismatch("assignment", "", destination.type, *value);

      const auto slots = m_types.slot_count(destination.type);
      emit_store(destination.slot, slots);
      if (keep_value) emit_load(destination.slot, slots);
      return destination.type;
    }

    constexpr auto compile_expr_node(const nodes::expr_member &member) -> result_t {
      auto object = resolve_place(member.object);
      if (not object) return std::unexpected{object.error()};

      // A chain rooted at a variable: the member is a fixed slot range of the frame
      if (*object) {
        auto field = lookup_field((*object)->type, member.field_name);
        if (not field) return std::unexpected{field.error()};

        emit_load((*object)->slot + field->slot_offset, m_types.slot_count(field->type));
        return field->type;
      }

      // Member of a temporary: compute the whole value, keep the member
      auto type = compile_expr(member.object);
      if (not type) return type;

      auto field = lookup_field(*type, member.field_name);
      if (not field) return std::unexpected{field.error()};

      m_builder.emit_pick(static_cast<vm::slot_t>(m_types.slot_count(*type)),
                          static_cast<vm::slot_t>(field->slot_offset),
                          static_cast<vm::slot_t>(m_types.slot_count(field->type)));
      return field->type;
    }

    constexpr auto compile_expr_node(const nodes::expr_call &call) -> result_t {
      if (const function_info *script = m_symbols.lookup_function(call.name)) {
        if (auto ok = compile_arguments(call, script->param_types); not ok) return std::unexpected{ok.error()};

        m_builder.emit_call(script->label, static_cast<vm::slot_t>(script->param_slots));
        return script->return_type;
      }

      auto bound = m_bindings.get(call.name);
      if (not bound) {
        return std::unexpected{error::undefined_symbol{.identifier = call.name}};
      }

      const std::span<const type_info> params{bound->param_types.data(), bound->param_count};
      if (auto ok = compile_arguments(call, params); not ok) return std::unexpected{ok.error()};

      m_builder.emit_trap(bound->id);
      return bound->return_type;
    }

    // Pushes every argument once, checking its type as soon as it is compiled
    constexpr auto compile_arguments(const nodes::expr_call &call, std::span<const type_info> params) -> status_t {
      std::size_t count = 0;
      for ([[maybe_unused]] auto arg: nodes::get_list_view(m_nodes, call.args_head)) ++count;

      if (count != params.size()) {
        return std::unexpected{error::argument_count_mismatch{
          .function_name = call.name, .expected = params.size(), .actual = count
        }};
      }

      std::size_t i = 0;
      for (auto arg: nodes::get_list_view(m_nodes, call.args_head)) {
        auto type = compile_expr(arg);
        if (not type) return std::unexpected{type.error()};
        if (*type != params[i]) return mismatch("argument", call.name, params[i], *type);
        ++i;
      }
      return {};
    }

    // An operand of an arithmetic, comparison or logical operator has to be an int
    constexpr auto compile_operand(nodes::index_t idx) -> status_t {
      auto type = compile_expr(idx);
      if (not type) return std::unexpected{type.error()};
      if (not type->is_i64()) return mismatch("operand", "", type_info::i64(), *type);
      return {};
    }

    // ---------------------------------------------------------------------------------------
    // Places
    // ---------------------------------------------------------------------------------------

    // Where a variable or a member chain rooted at a variable lives; nullopt for anything else
    constexpr auto resolve_place(nodes::index_t idx) const -> std::expected<std::optional<place>, error_t> {
      const auto &data = m_nodes[idx].data;

      if (const auto *var = std::get_if<nodes::expr_var>(&data)) {
        auto info = m_symbols.lookup_variable(var->name);
        if (not info) {
          return std::unexpected{error::undefined_symbol{.identifier = var->name}};
        }
        return std::optional{place{info->first_slot, info->type}};
      }

      if (const auto *member = std::get_if<nodes::expr_member>(&data)) {
        auto object = resolve_place(member->object);
        if (not object) return object;
        if (not *object) return std::optional<place>{};

        auto field = lookup_field((*object)->type, member->field_name);
        if (not field) return std::unexpected{field.error()};
        return std::optional{place{(*object)->slot + field->slot_offset, field->type}};
      }

      return std::optional<place>{};
    }

    constexpr auto lookup_field(const type_info &type, std::string_view name) const
      -> std::expected<field_info, error_t> {
      if (not type.is_struct()) {
        return std::unexpected{error::member_on_non_struct{.type_name = type_name(type)}};
      }

      auto field = m_types.find_field(type, name);
      if (not field) {
        return std::unexpected{error::unknown_field{.struct_name = type.struct_name, .field_name = name}};
      }
      return *field;
    }

    // ---------------------------------------------------------------------------------------
    // Emit helpers
    // ---------------------------------------------------------------------------------------

    constexpr auto emit_load(std::size_t slot, std::size_t count) -> void {
      if (count == 1) {
        m_builder.emit_load_local(static_cast<vm::slot_t>(slot));
      } else if (count > 1) {
        m_builder.emit_load_locals(static_cast<vm::slot_t>(slot), static_cast<vm::slot_t>(count));
      }
    }

    constexpr auto emit_store(std::size_t slot, std::size_t count) -> void {
      if (count == 1) {
        m_builder.emit_save_local(static_cast<vm::slot_t>(slot));
      } else if (count > 1) {
        m_builder.emit_save_locals(static_cast<vm::slot_t>(slot), static_cast<vm::slot_t>(count));
      }
    }

    // `and` / `or` are not here, they are compiled by `compile_logical`
    static constexpr auto arithmetic_op_code(nodes::binary_op op) -> vm::op_code {
      switch (op) {
        case nodes::binary_op::add: return vm::op_code::i64_add;
        case nodes::binary_op::sub: return vm::op_code::i64_sub;
        case nodes::binary_op::mul: return vm::op_code::i64_mul;
        case nodes::binary_op::div: return vm::op_code::i64_div;
        case nodes::binary_op::mod: return vm::op_code::i64_mod;
        case nodes::binary_op::eq: return vm::op_code::i64_eq;
        case nodes::binary_op::ne: return vm::op_code::i64_ne;
        case nodes::binary_op::lt: return vm::op_code::i64_lt;
        case nodes::binary_op::le: return vm::op_code::i64_le;
        case nodes::binary_op::gt: return vm::op_code::i64_gt;
        case nodes::binary_op::ge: return vm::op_code::i64_ge;
        case nodes::binary_op::and_:
        case nodes::binary_op::or_:
          break;
      }
      std::unreachable();
    }

    static constexpr auto mismatch(std::string_view context, std::string_view subject,
                                   const type_info &expected, const type_info &actual) -> std::unexpected<error_t> {
      return std::unexpected{error_t{error::type_mismatch{
        .context = context,
        .subject = subject,
        .expected = type_name(expected),
        .actual = type_name(actual)
      }}};
    }
  };

  template<auto &&nodes, nodes::index_t root, auto bindings>
  consteval static auto compile_nodes() {
    using exposed_types = typename std::remove_cvref_t<decltype(*bindings)>::exposed_types;

    constexpr static auto expected = [] { return compiler{nodes, root, *bindings}.compile(); };

    if constexpr (not expected()) {
      report_error<[] { return expected().error(); }>();
      return expected().error();
    } else {
      return compilation_result_to_const<[] constexpr { return expected().value(); }, exposed_types>();
    }
  }

  template<const_string code, auto bindings>
  consteval static auto compile() {
    constexpr static auto nodes_root = parse<code>();

    return compile_nodes<nodes_root.first, nodes_root.second, bindings>();
  }
} // namespace korka
