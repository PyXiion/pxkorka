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
      if (not first_id_token or first_id_token->kind != lex_kind::kIdentifier) {
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

    constexpr auto try_parse_declaration_in_block() -> index_t {
      // To rollback in case we fail
      auto start = m_current;

      auto type = parse_type_specifier();
      if (not type) {
        return empty_node;
      }

      auto name = parse_id();
      if (not name) {
        m_current = start;
        return empty_node;
      }

      if (not match(lex_kind::kSemicolon)) {
        m_current = start;
        return empty_node;
      }

      return m_pool.add(decl_var{*type, *name, empty_node});
    }

    constexpr auto parse_statement() -> index_t {
      if (not peek())
        return empty_node;

      switch (peek()->kind) {
        case lex_kind::kOpenBrace:
          return parse_compound_stmt();
        case lex_kind::kIf:
          return parse_if_statement();
        default:
          return parse_expression_stmt();
      }

      // TODO expression_stmt, if_stmt, while_stmt, return_stmt
      return empty_node;
    }

    constexpr auto parse_if_statement() -> index_t {
      if (not match(lex_kind::kIf) or not match(lex_kind::kOpenParenthesis)) {
        return empty_node;
      }
//      auto expression_idx = parse_expression();
//      if (expression_idx == empty_node) {
//        return empty_node;
//      }

      if (not match(lex_kind::kCloseParenthesis)) {
        return empty_node;
      }

      auto stmt = parse_statement();
      if (stmt == empty_node) {
        return empty_node;
      }

      auto else_stmt = empty_node;
      if (match(lex_kind::kElse)) {
        else_stmt = parse_statement();
      }

      return m_pool.add(stmt_if{
        .condition = empty_node, // TODO
        .then_branch = stmt,
        .else_branch = else_stmt
      });
    }

    constexpr auto parse_compound_stmt() -> index_t {
      if (not match(lex_kind::kOpenBrace)) return empty_node;

      auto get_decl_or_stmt = [&] {
        auto node_idx = try_parse_declaration_in_block();
        if (node_idx == empty_node) {
          node_idx = parse_statement();
        }
        return node_idx;
      };

      auto head = get_decl_or_stmt();

      for (auto idx = get_decl_or_stmt(); idx != empty_node; idx = get_decl_or_stmt()) {
        m_pool.append_list(head, idx);
      }

      if (not match(lex_kind::kCloseBrace)) {
        // TODO: error
        return empty_node;
      }

      return m_pool.add(stmt_block{head});
    }

    constexpr auto parse_expression_stmt() -> index_t {
      if (match(lex_kind::kSemicolon)) {
        // empty expression statement
        return m_pool.add(stmt_expr{empty_node});
      }
      auto expr = parse_expression();
      if (expr == empty_node) return empty_node;
      if (!match(lex_kind::kSemicolon)) return empty_node;
      return m_pool.add(stmt_expr{expr});
    }

    constexpr auto parse_expression() -> index_t {
      return parse_assignment();
    }

    constexpr auto parse_assignment() -> index_t {
      // Look ahead for identifier followed by '=' (assignment)
      if (auto tok = peek(); tok and tok->kind == lex_kind::kIdentifier) {
        // check next token
        if (auto next = peek_next(); next and next->kind == lex_kind::kEqual) // '='
        {
          auto id_tok = advance(); // consume identifier
          advance();                                 // consume '='
          auto right = parse_assignment();
          if (right == empty_node) return empty_node;
          auto var_idx = m_pool.add(expr_var{id_tok->lexeme});
          return m_pool.add(expr_binary{"=", var_idx, right});
        }
      }
      // Otherwise fall back to logical-or
      return parse_logical_or();
    }

    constexpr auto parse_logical_or() -> index_t {
      auto left = parse_logical_and();
      while (true) {
        auto tok = peek();
        if (!tok or tok->kind != lex_kind::kOr) break;   // "or"
        advance();
        auto right = parse_logical_and();
        if (right == empty_node) return empty_node;
        left = m_pool.add(expr_binary{tok->lexeme, left, right});
      }
      return left;
    }

    constexpr auto parse_logical_and() -> index_t {
      auto left = parse_equality();
      while (true) {
        auto tok = peek();
        if (!tok || tok->kind != lex_kind::kAnd) break;   // "and"
        advance();
        auto right = parse_equality();
        if (right == empty_node) return empty_node;
        left = m_pool.add(expr_binary{tok->lexeme, left, right});
      }
      return left;
    }

    constexpr auto parse_equality() -> index_t {
      auto left = parse_relational();
      while (true) {
        auto tok = peek();
        if (!tok) break;
        if (tok->kind != lex_kind::kEqualEqual &&      // "=="
            tok->kind != lex_kind::kBangEqual)
          break;   // "!="
        advance();
        auto right = parse_relational();
        if (right == empty_node) return empty_node;
        left = m_pool.add(expr_binary{tok->lexeme, left, right});
      }
      return left;
    }

    constexpr auto parse_relational() -> index_t {
      auto left = parse_additive();
      while (true) {
        auto tok = peek();
        if (!tok) break;
        if (tok->kind != lex_kind::kLess &&            // "<"
            tok->kind != lex_kind::kGreater &&         // ">"
            tok->kind != lex_kind::kLessEqual &&       // "<="
            tok->kind != lex_kind::kGreaterEqual)
          break; // ">="
        advance();
        auto right = parse_additive();
        if (right == empty_node) return empty_node;
        left = m_pool.add(expr_binary{tok->lexeme, left, right});
      }
      return left;
    }

    constexpr auto parse_additive() -> index_t {
      auto left = parse_multiplicative();
      while (true) {
        auto tok = peek();
        if (!tok) break;
        if (tok->kind != lex_kind::kPlus &&            // "+"
            tok->kind != lex_kind::kMinus)
          break;      // "-"
        advance();
        auto right = parse_multiplicative();
        if (right == empty_node) return empty_node;
        left = m_pool.add(expr_binary{tok->lexeme, left, right});
      }
      return left;
    }

    constexpr auto parse_multiplicative() -> index_t {
      auto left = parse_unary();
      while (true) {
        auto tok = peek();
        if (!tok) break;
        if (tok->kind != lex_kind::kStar &&            // "*"
            tok->kind != lex_kind::kSlash &&           // "/"
            tok->kind != lex_kind::kPercent)
          break;    // "%"
        advance();
        auto right = parse_unary();
        if (right == empty_node) return empty_node;
        left = m_pool.add(expr_binary{tok->lexeme, left, right});
      }
      return left;
    }

    constexpr auto parse_unary() -> index_t {
      auto tok = peek();
      if (tok && (tok->kind == lex_kind::kPlus ||
                  tok->kind == lex_kind::kMinus ||
                  tok->kind == lex_kind::kBang))          // "!", "+", "-"
      {
        advance();
        auto child = parse_unary();
        if (child == empty_node) return empty_node;
        return m_pool.add(expr_unary{tok->lexeme, child});
      }
      return parse_primary();
    }

    constexpr auto parse_primary() -> index_t {
      auto tok = peek();
      if (!tok) return empty_node;

      switch (tok->kind) {
        case lex_kind::kIdentifier: {
          auto id_tok = advance();
          // function call if next token is '('
          if (auto next = peek(); next && next->kind == lex_kind::kOpenParenthesis) {
            return parse_func_call(id_tok->lexeme);
          }
          // otherwise variable reference
          return m_pool.add(expr_var{id_tok->lexeme});
        }

        case lex_kind::kNumberLiteral: {
          auto num_tok = advance();
          // convert lexeme to integer (simple constexpr conversion)
          int value = 0;
          for (char c: num_tok->lexeme) {
            if (c < '0' || c > '9') break;   // should not happen
            value = value * 10 + (c - '0');
          }
          return m_pool.add(expr_literal{value});
        }

        case lex_kind::kOpenParenthesis: {
          advance();   // '('
          auto expr = parse_expression();
          if (expr == empty_node) return empty_node;
          if (!match(lex_kind::kCloseParenthesis)) return empty_node;
          return expr;
        }

        default:
          return empty_node;
      }
    }

    constexpr auto parse_func_call(std::string_view name) -> index_t {
      // '(' already seen (caller ensures next token is '(')
      if (!match(lex_kind::kOpenParenthesis)) return empty_node;
      auto args_head = parse_argument_list();
      if (!match(lex_kind::kCloseParenthesis)) return empty_node;
      return m_pool.add(expr_call{name, args_head});
    }

    constexpr auto parse_argument_list() -> index_t {
      // Check for empty list
      if (auto tok = peek(); !tok || tok->kind == lex_kind::kCloseParenthesis) {
        return empty_node;
      }

      index_t head = empty_node;
      index_t tail = empty_node;
      while (true) {
        auto expr = parse_expression();
        if (expr == empty_node) return empty_node;   // error

        if (head == empty_node) {
          head = expr;
        } else {
          m_pool.nodes[tail].next = expr;
        }
        tail = expr;

        auto tok = peek();
        if (tok && tok->kind == lex_kind::kComma) {
          advance();
          continue;
        }
        break;
      }
      return head;
    }

    constexpr auto is_at_end(std::int32_t offset = 0) const -> bool {
      return m_current + offset >= m_tokens.size() and m_current + offset > 0;
    }

    constexpr auto peek(std::int32_t offset = 0) -> std::optional<lex_token> {
      if (is_at_end(offset)) return std::nullopt;
      return m_tokens[m_current + offset];
    }

    constexpr auto peek_next() -> std::optional<lex_token> {
      return peek(1);
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
  constexpr auto parse_tokens() {
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