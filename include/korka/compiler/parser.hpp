#pragma once

#include "korka/shared.hpp"
#include "lexer.hpp"
#include <expected>
#include <variant>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace korka {
  class parser {
  public:
    using index_t = int32_t;
    constexpr static index_t empty_node = -1;

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

    struct decl_program {
      index_t external_declarations_head; ///< decl_function, decl_global linked list
    };


    struct node {
      using data_t = std::variant<
        // Expressions
        expr_literal, expr_var, expr_unary, expr_binary, expr_call,
        // Statements
        stmt_block, stmt_if, stmt_while, stmt_return, stmt_expr, decl_var,
        // Top-level
        decl_function, decl_program
      >;

      data_t data;

      // Allows to make lists without allocations,
      // because it would be hard to extract nodes
      // into runtime (for testing and printing)
      // if they had vectors or pointers (std::unique_ptr
      // is allowed inside constexpr)
      index_t next = empty_node;
    };

    struct ast_pool {
      std::vector<node> nodes{};
      size_t count = 0;

      template<typename T>
      constexpr auto add(T &&data) -> index_t {
        nodes.emplace_back(std::forward<T>(data), empty_node);
        return static_cast<index_t>(count++);
      }

      constexpr auto append_list(index_t head, index_t new_node) -> void {
        if (head == empty_node) return;
        index_t current = head;
        while (nodes[current].next != empty_node) {
          current = nodes[current].next;
        }
        nodes[current].next = new_node;
      }
    };

  public:
    constexpr explicit parser(std::span<const lex_token> tokens) : m_tokens(tokens) {};

    constexpr auto parse() -> std::pair<std::vector<node>, index_t> {
      auto decl = parse_external_declaration();
      auto head = decl;

      decl = parse_external_declaration();
      for (; decl != empty_node; decl = parse_external_declaration()) {
        m_pool.append_list(head, decl);
      }

      return std::make_pair(std::move(m_pool.nodes), head);
    }

  private:
    std::span<const lex_token> m_tokens;
    ast_pool m_pool{};

    std::size_t m_current{0};

    constexpr auto parse_external_declaration() -> index_t {
      auto type = parse_type_specifier();
      if (not type) {
        return empty_node;
      }

      auto first_id_token = advance();
      if (not first_id_token || first_id_token->kind != lex_kind::kIdentifier) {
        return empty_node;
      }

      if (peek()->kind != lex_kind::kOpenParenthesis) {
        // TODO global_declaration
        return empty_node;
      }

      // Function
      if (not match(lex_kind::kOpenParenthesis)) {
        return empty_node;
      }

      // TODO: parameters

      if (not match(lex_kind::kCloseParenthesis)) {
        return empty_node;
      }

      auto cmpd_index = parse_compound_stmt();
      if (cmpd_index == empty_node) {
        return empty_node;
      }

      auto info = decl_function{
        .ret_type = *type,
        .name = first_id_token->lexeme,
        .params_head = empty_node,
        .body = cmpd_index,
      };

      return m_pool.add(info);
    };

    constexpr auto parse_type_specifier() -> std::optional<std::string_view> {
      auto token = peek();

      if (token->kind != lex_kind::kInt && token->kind != lex_kind::kIdentifier) {
        return std::nullopt;
      }
      advance();

      return token->lexeme;
    }

    constexpr auto parse_id() -> std::optional<std::string_view> {
      auto token = peek();

      if (token->kind != lex_kind::kIdentifier) {
        return std::nullopt;
      }
      advance();

      return token->lexeme;
    }

    constexpr auto parse_declaration_in_block() -> index_t {
      auto type = parse_type_specifier();
      if (not type) {
        return empty_node;
      }

      auto name = parse_id();
      if (not name) {
        return empty_node;
      }

      if (not match(lex_kind::kSemicolon)) {
        return empty_node;
      }

      return m_pool.add(decl_var{*type, *name, empty_node});
    }

    constexpr auto parse_statement() -> index_t {
      if (not peek())
        return empty_node;

      if (peek()->kind == lex_kind::kOpenBrace) {
        return parse_compound_stmt();
      }
      // TODO expression_stmt, if_stmt, while_stmt, return_stmt
      return empty_node;
    }

    constexpr auto parse_compound_stmt() -> index_t {
      if (not match(lex_kind::kOpenBrace)) return empty_node;

      auto var_decl = parse_declaration_in_block();
      auto head = var_decl;

      var_decl = parse_declaration_in_block();
      for (; var_decl != empty_node; var_decl = parse_declaration_in_block()) {
        m_pool.append_list(head, var_decl);
      }

      auto stmt = parse_statement();

      stmt = parse_statement();
      for (; stmt != empty_node; stmt = parse_statement()) {
        m_pool.append_list(head, stmt);
      }

      if (not match(lex_kind::kCloseBrace)) {
        // TODO: error
        return empty_node;
      }

      return m_pool.add(stmt_block{head});
    }

    constexpr auto parse_expression_stmt() -> index_t {

      return m_pool.add(stmt_expr{});
    }

    constexpr auto parse_expression() -> index_t {
      return empty_node;
    }

    constexpr auto is_at_end() const -> bool {
      return m_current >= m_tokens.size();
    }

    constexpr auto peek() -> std::optional<lex_token> {
      if (is_at_end()) return std::nullopt;
      return m_tokens[m_current];
    }

    constexpr auto advance() -> std::optional<lex_token> {
      if (is_at_end()) return std::nullopt;
      return m_tokens[m_current++];
    }

    constexpr auto match(lex_kind kind) -> std::optional<lex_token> {
      auto token = peek();
      if (not token) return std::nullopt;
      if (token->kind == kind) {
        advance();
        return token;
      }
      return std::nullopt;
    }
  };

  template<const auto &tokens>
  auto parse_tokens() {
    constexpr static auto p = [] constexpr {
      return parser{std::span{tokens}}.parse();
    };
    constexpr static auto p_array = [] constexpr {
      return p().first;
    };
    return std::make_pair(to_array<p_array>(), p().second);
  }

  template<const_string code>
  constexpr auto parse() {
    return parse_tokens<lexer{code}.lex()>();
  }
}