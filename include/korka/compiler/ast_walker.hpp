#pragma once
#include "parser.hpp"
#include "korka/utils/overloaded.hpp"
#include <format>
#include <span>

namespace korka {
  struct ast_walker {
    const std::span<const nodes::node> &pool;
    nodes::index_t index;
    int indent = 0;
  };
}

template<>
struct std::formatter<korka::ast_walker> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const korka::ast_walker& w, std::format_context& ctx) const {
    auto out = ctx.out();

    if (w.index == korka::nodes::empty_node) {
      return std::format_to(out, "{}<null>", std::string(w.indent * 2, ' '));
    }

    if (w.index < 0 || w.index >= static_cast<int>(w.pool.size())) {
      return std::format_to(out, "{}<INVALID INDEX {}>", std::string(w.indent * 2, ' '), w.index);
    }

    const auto& node = w.pool[w.index];
    std::string spaces(w.indent * 2, ' ');

    auto fmt_child = [&](std::string_view label, korka::nodes::index_t child_idx) {
      std::format_to(out, "\n{}{}:", spaces, label);
      std::format_to(out, "\n{}", korka::ast_walker{w.pool, child_idx, w.indent + 1});
    };

    std::format_to(out, "{}", spaces);

    using namespace korka;
    std::visit(overloaded{
      [&](const nodes::expr_literal& lit) {
        std::visit(overloaded{
          [&](std::monostate) {
            out = std::format_to(out, "null");
          },
          [&](auto &&v) {
            out = std::format_to(out, "{}", v);
          }
        }, lit);
      },
      [&](const nodes::expr_var& v) {
        out = std::format_to(out, "Var '{}'", v.name);
      },
      [&](const nodes::expr_unary& v) {
        out = std::format_to(out, "Unary '{}'", v.op);
        fmt_child("child", v.child);
      },
      [&](const nodes::expr_binary& v) {
        out = std::format_to(out, "Binary '{}'", v.op);
        fmt_child("L", v.left);
        fmt_child("R", v.right);
      },
      [&](const nodes::expr_call& v) {
        out = std::format_to(out, "Call '{}'", v.name);
        if (v.args_head != nodes::empty_node) {
          fmt_child("args", v.args_head);
        }
      },
      [&](const nodes::stmt_block& v) {
        out = std::format_to(out, "Block");
        if (v.children_head != nodes::empty_node) {
          fmt_child("body", v.children_head);
        }
      },
      [&](const nodes::stmt_if& v) {
        out = std::format_to(out, "If");
        fmt_child("cond", v.condition);
        fmt_child("then", v.then_branch);
        if (v.else_branch != nodes::empty_node) {
          fmt_child("else", v.else_branch);
        }
      },
      [&](const nodes::stmt_while& v) {
        out = std::format_to(out, "While");
        fmt_child("cond", v.condition);
        fmt_child("body", v.body);
      },
      [&](const nodes::stmt_return& v) {
        out = std::format_to(out, "Return");
        if (v.expr != nodes::empty_node) fmt_child("val", v.expr);
      },
      [&](const nodes::stmt_expr& v) {
        out = std::format_to(out, "ExprStmt");
        fmt_child("expr", v.expr);
      },
      [&](const nodes::decl_var& v) {
        out = std::format_to(out, "DeclVar '{} {}'", v.type_name, v.var_name);
        if (v.init_expr != nodes::empty_node) {
          fmt_child("init", v.init_expr);
        }
      },
      [&](const nodes::decl_function& v) {
        out = std::format_to(out, "Function '{} {}'", v.ret_type, v.name);
        if (v.params_head != nodes::empty_node) fmt_child("params", v.params_head);
        fmt_child("body", v.body);
      },
      [&](const nodes::decl_program& v) {
        out = std::format_to(out, "Program");
        fmt_child("roots", v.external_declarations_head);
      }
    }, node.data);


    if (node.next != nodes::empty_node) {
      std::format_to(out, "\n{}", korka::ast_walker{w.pool, node.next, w.indent});
    }

    return out;
  }
};