#pragma once

#include "korka/shared.hpp"
#include <variant>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace korka {
  using index_t = int32_t;
  constexpr index_t none = -1;
  constexpr index_t max_args = 10;

  // EXPRESSIONS

  using expr_literal = literal_value_t;

  struct expr_var {
    std::string_view name;
  };

  struct expr_unary {
    std::string_view op; // "-", "!", "+"
    index_t child;
  };

  struct expr_binary {
    std::string_view op; // "+", "==", "&&", "=", etc
    index_t left;
    index_t right;
  };

  struct expr_call {
    std::string_view name;
    index_t args_head; ///< linked list of expressions
  };

  // STATEMENTS

  struct stmt_block {
    index_t children_head; ///< linked list of children
  };

  struct stmt_if {
    index_t condition;
    index_t then_branch;
    index_t else_branch; ///< can be none
  };

  struct stmt_while {
    index_t condition;
    index_t body;
  };

  struct stmt_return {
    index_t expr; ///< can be none (for void)
  };

  struct stmt_expr {
    index_t expr; ///< expression used as an instrucction, smth like "i++;"
  };

  /// Var declaration, used both in instructions and function parameters
  struct decl_var {
    std::string_view type_name;
    std::string_view var_name;
    index_t init_expr; ///< can be none
  };

  // TOP-LEVEL

  struct decl_function {
    std::string_view ret_type;
    std::string_view name;

    index_t params_head; // decl_var linked list
    index_t body;        // stmt_block
  };



  struct node {
    using data_t = std::variant<
      // Root (or empty)
      std::monostate,
      // Expressions
      expr_literal, expr_var, expr_unary, expr_binary, expr_call,
      // Statements
      stmt_block, stmt_if, stmt_while, stmt_return, stmt_expr, decl_var,
      // Top-level
      decl_function
    >;

    data_t data;

    // Allows to make lists without allocations,
    // because it would be hard to extract nodes
    // into runtime (for testing and printing)
    // if they had vectors or pointers (std::unique_ptr
    // is allowed inside constexpr)
    index_t next = none;
  };

  struct ast_pool {
    std::vector<node> nodes{};
    size_t count = 0;

    template<typename T>
    constexpr index_t add(T &&data) {
      nodes[count] = node{std::forward<T>(data), none};
      return static_cast<index_t>(count++);
    }

    constexpr void append_list(index_t head, index_t new_node) {
      if (head == none) return;
      index_t current = head;
      while (nodes[current].next != none) {
        current = nodes[current].next;
      }
      nodes[current].next = new_node;
    }
  };
}