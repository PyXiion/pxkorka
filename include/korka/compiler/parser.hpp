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

    // --- Node Structures ---
    using expr_literal = literal_value_t;
    struct expr_var { std::string_view name; };
    struct expr_unary { std::string_view op; index_t child; };
    struct expr_binary { std::string_view op; index_t left; index_t right; };
    struct expr_call { std::string_view name; index_t args_head; };
    struct stmt_block { index_t children_head; };
    struct stmt_if { index_t condition; index_t then_branch; index_t else_branch; };
    struct stmt_while { index_t condition; index_t body; };
    struct stmt_return { index_t expr; };
    struct stmt_expr { index_t expr; };
    struct decl_var { std::string_view type_name; std::string_view var_name; index_t init_expr; };
    struct decl_function { std::string_view ret_type; std::string_view name; index_t params_head; index_t body; };
    struct decl_program { index_t external_declarations_head; };

    struct node {
      using data_t = std::variant<
        expr_literal, expr_var, expr_unary, expr_binary, expr_call,
        stmt_block, stmt_if, stmt_while, stmt_return, stmt_expr, decl_var,
        decl_function, decl_program
      >;
      data_t data;
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
        if (head == empty_node || new_node == empty_node) return;
        index_t current = head;
        while (nodes[current].next != empty_node) {
          current = nodes[current].next;
        }
        nodes[current].next = new_node;
      }
    };

  public:
    using parse_result = std::expected<index_t, error_t>;

    constexpr explicit parser(std::span<const lex_token> tokens) : m_tokens(tokens) {};

    constexpr auto parse() -> std::expected<std::pair<std::vector<node>, index_t>, error_t> {
      if (auto tok = peek(); tok and tok->kind == lex_kind::kEof) {
        index_t root = m_pool.add(decl_program{empty_node});
        return std::make_pair(std::move(m_pool.nodes), root);
      }
      
      auto head = parse_external_declaration();
      if (!head) return std::unexpected{head.error()};

      while (peek()) {
        if (peek()->kind == lex_kind::kEof) {
          break;
        }
        auto decl = parse_external_declaration();
        if (not decl) return std::unexpected{decl.error()};
        if (*decl == empty_node) break;

        m_pool.append_list(*head, *decl);
      }

      index_t root = m_pool.add(decl_program{*head});
      return std::make_pair(std::move(m_pool.nodes), root);
    }

  private:
    std::span<const lex_token> m_tokens;
    ast_pool m_pool{};
    std::size_t m_current{0};

    constexpr auto parse_external_declaration() -> parse_result {
      auto type = parse_type_specifier();
      if (not type) return std::unexpected{type.error()};

      auto name = parse_id();
      if (not name) return std::unexpected{name.error()};

      if (match(lex_kind::kOpenParenthesis)) {
        auto params = parse_parameter_list();
        if (!params) return std::unexpected{params.error()};

        if (!match(lex_kind::kCloseParenthesis))
          return make_error("Expected ')' after parameters");

        auto body = parse_compound_stmt();
        if (!body) return std::unexpected{body.error()};

        return m_pool.add(decl_function{
          .ret_type = *type,
          .name = *name,
          .params_head = *params,
          .body = *body
        });
      }

      return make_error("Global variables not implemented yet");
    };

    constexpr auto parse_parameter_list() -> parse_result {
      if (auto next = peek(); next && next->kind == lex_kind::kCloseParenthesis) {
        return empty_node;
      }

      auto first = parse_parameter_decl();
      if (!first) return std::unexpected{first.error()};

      index_t head = *first;
      while (match(lex_kind::kComma)) {
        auto next = parse_parameter_decl();
        if (!next) return std::unexpected{next.error()};
        m_pool.append_list(head, *next);
      }
      return head;
    }

    constexpr auto parse_parameter_decl() -> parse_result {
      auto type = parse_type_specifier();
      if (not type) return std::unexpected{type.error()};

      auto name = parse_id();
      if (!name) return std::unexpected{name.error()};

      return m_pool.add(decl_var{*type, *name, empty_node});
    }

    constexpr auto parse_type_specifier() -> std::expected<std::string_view, error_t> {
      auto token = peek();
      if (!token) return make_error("Expected type specifier");

      if (token->kind != lex_kind::kInt && token->kind != lex_kind::kIdentifier) {
        return make_error("Expected builtin type or type identifier");
      }
      advance();
      return token->lexeme;
    }

    constexpr auto parse_id() -> std::expected<std::string_view, error_t> {
      auto token = peek();
      if (!token || token->kind != lex_kind::kIdentifier) {
        return make_error("Expected identifier");
      }
      advance();
      return token->lexeme;
    }

    constexpr auto try_parse_declaration_in_block() -> parse_result {
      auto start = m_current;
      auto type = parse_type_specifier();
      if (!type) {
        m_current = start;
        return std::unexpected{type.error()};
      }

      auto name = parse_id();
      if (!name) {
        m_current = start;
        return std::unexpected{name.error()};
      }

      index_t init_expr = empty_node;
      if (match(lex_kind::kEqual)) {
        auto expr = parse_expression();
        if (!expr) return std::unexpected{expr.error()};
        init_expr = *expr;
      }

      if (!match(lex_kind::kSemicolon)) {
        m_current = start;
        return make_error("Expected ';' after variable declaration");
      }

      return m_pool.add(decl_var{*type, *name, init_expr});
    }

    constexpr auto parse_statement() -> parse_result {
      auto tok = peek();
      if (!tok) return make_error("Unexpected end of input");

      switch (tok->kind) {
        case lex_kind::kOpenBrace: return parse_compound_stmt();
        case lex_kind::kIf:        return parse_if_statement();
        case lex_kind::kWhile:     return parse_while_statement();
        case lex_kind::kReturn:    return parse_return_statement();
        default:                   return parse_expression_stmt();
      }
    }

    constexpr auto parse_return_statement() -> parse_result {
      if (!match(lex_kind::kReturn)) return make_error("Expected 'return'");

      index_t expr_idx = empty_node;
      if (auto next = peek(); next && next->kind != lex_kind::kSemicolon) {
        auto expr = parse_expression();
        if (!expr) return std::unexpected{expr.error()};
        expr_idx = *expr;
      }

      if (!match(lex_kind::kSemicolon)) return make_error("Expected ';' after return");
      return m_pool.add(stmt_return{expr_idx});
    }

    constexpr auto parse_while_statement() -> parse_result {
      if (!match(lex_kind::kWhile)) return make_error("Expected 'while'");
      if (!match(lex_kind::kOpenParenthesis)) return make_error("Expected '('");

      auto cond = parse_expression();
      if (!cond) return std::unexpected{cond.error()};

      if (!match(lex_kind::kCloseParenthesis)) return make_error("Expected ')'");

      auto body = parse_statement();
      if (!body) return std::unexpected{body.error()};

      return m_pool.add(stmt_while{*cond, *body});
    }

    constexpr auto parse_if_statement() -> parse_result {
      if (!match(lex_kind::kIf)) return make_error("Expected 'if'");
      if (!match(lex_kind::kOpenParenthesis)) return make_error("Expected '('");

      auto cond = parse_expression();
      if (!cond) return std::unexpected{cond.error()};

      if (!match(lex_kind::kCloseParenthesis)) return make_error("Expected ')'");

      auto then_b = parse_statement();
      if (!then_b) return std::unexpected{then_b.error()};

      index_t else_b = empty_node;
      if (match(lex_kind::kElse)) {
        auto res = parse_statement();
        if (!res) return std::unexpected{res.error()};
        else_b = *res;
      }

      return m_pool.add(stmt_if{*cond, *then_b, else_b});
    }

    constexpr auto parse_compound_stmt() -> parse_result {
      if (!match(lex_kind::kOpenBrace)) return make_error("Expected '{'");

      index_t head = empty_node;

      while (true) {
        auto tok = peek();
        if (!tok || tok->kind == lex_kind::kCloseBrace) break;
        if (tok->kind == lex_kind::kEof) return make_error("Expected '}'");

        // Try declaration first
        auto node_res = try_parse_declaration_in_block();

        if (!node_res) {
          // If try_parse failed, it might be a statement
          node_res = parse_statement();
        }

        if (!node_res) return node_res;

        if (head == empty_node) head = *node_res;
        else m_pool.append_list(head, *node_res);
      }

      if (!match(lex_kind::kCloseBrace)) return make_error("Expected '}'");
      return m_pool.add(stmt_block{head});
    }

    constexpr auto parse_expression_stmt() -> parse_result {
      if (match(lex_kind::kSemicolon)) return m_pool.add(stmt_expr{empty_node});

      auto expr = parse_expression();
      if (!expr) return std::unexpected{expr.error()};

      if (!match(lex_kind::kSemicolon)) return make_error("Expected ';' after expression");
      return m_pool.add(stmt_expr{*expr});
    }

    constexpr auto parse_expression() -> parse_result { return parse_assignment(); }

    constexpr auto parse_assignment() -> parse_result {
      if (auto tok = peek(); tok && tok->kind == lex_kind::kIdentifier) {
        if (auto next = peek_next(); next && next->kind == lex_kind::kEqual) {
          auto id_tok = advance();
          advance(); // '='
          auto right = parse_assignment();
          if (!right) return std::unexpected{right.error()};
          auto var_idx = m_pool.add(expr_var{id_tok->lexeme});
          return m_pool.add(expr_binary{"=", var_idx, *right});
        }
      }
      return parse_logical_or();
    }

    constexpr auto parse_logical_or() -> parse_result {
      auto left = parse_logical_and();
      if (!left) return left;
      while (auto tok = match(lex_kind::kOr)) {
        auto right = parse_logical_and();
        if (!right) return std::unexpected{right.error()};
        left = m_pool.add(expr_binary{tok->lexeme, *left, *right});
      }
      return left;
    }

    constexpr auto parse_logical_and() -> parse_result {
      auto left = parse_equality();
      if (!left) return left;
      while (auto tok = match(lex_kind::kAnd)) {
        auto right = parse_equality();
        if (!right) return std::unexpected{right.error()};
        left = m_pool.add(expr_binary{tok->lexeme, *left, *right});
      }
      return left;
    }

    constexpr auto parse_equality() -> parse_result {
      auto left = parse_relational();
      if (!left) return left;
      while (true) {
        auto tok = peek();
        if (!tok || (tok->kind != lex_kind::kEqualEqual && tok->kind != lex_kind::kBangEqual)) break;
        advance();
        auto right = parse_relational();
        if (!right) return std::unexpected{right.error()};
        left = m_pool.add(expr_binary{tok->lexeme, *left, *right});
      }
      return left;
    }

    constexpr auto parse_relational() -> parse_result {
      auto left = parse_additive();
      if (!left) return left;
      while (true) {
        auto tok = peek();
        if (!tok || (tok->kind != lex_kind::kLess && tok->kind != lex_kind::kGreater &&
                     tok->kind != lex_kind::kLessEqual && tok->kind != lex_kind::kGreaterEqual)) break;
        advance();
        auto right = parse_additive();
        if (!right) return std::unexpected{right.error()};
        left = m_pool.add(expr_binary{tok->lexeme, *left, *right});
      }
      return left;
    }

    constexpr auto parse_additive() -> parse_result {
      auto left = parse_multiplicative();
      if (!left) return left;
      while (true) {
        auto tok = peek();
        if (!tok || (tok->kind != lex_kind::kPlus && tok->kind != lex_kind::kMinus)) break;
        advance();
        auto right = parse_multiplicative();
        if (!right) return std::unexpected{right.error()};
        left = m_pool.add(expr_binary{tok->lexeme, *left, *right});
      }
      return left;
    }

    constexpr auto parse_multiplicative() -> parse_result {
      auto left = parse_unary();
      if (!left) return left;
      while (true) {
        auto tok = peek();
        if (!tok || (tok->kind != lex_kind::kStar && tok->kind != lex_kind::kSlash && tok->kind != lex_kind::kPercent)) break;
        advance();
        auto right = parse_unary();
        if (!right) return std::unexpected{right.error()};
        left = m_pool.add(expr_binary{tok->lexeme, *left, *right});
      }
      return left;
    }

    constexpr auto parse_unary() -> parse_result {
      auto tok = peek();
      if (tok && (tok->kind == lex_kind::kPlus || tok->kind == lex_kind::kMinus || tok->kind == lex_kind::kBang)) {
        advance();
        auto child = parse_unary();
        if (!child) return std::unexpected{child.error()};
        return m_pool.add(expr_unary{tok->lexeme, *child});
      }
      return parse_primary();
    }

    constexpr auto parse_primary() -> parse_result {
      auto tok = peek();
      if (!tok) return make_error("Expected expression");

      switch (tok->kind) {
        case lex_kind::kIdentifier: {
          auto id_tok = advance();
          if (auto next = peek(); next && next->kind == lex_kind::kOpenParenthesis) {
            return parse_func_call(id_tok->lexeme);
          }
          return m_pool.add(expr_var{id_tok->lexeme});
        }
        case lex_kind::kStringLiteral:
        case lex_kind::kNumberLiteral: {
          auto lit_tok = advance();
          return m_pool.add(expr_literal{std::move(lit_tok)->value});
        }
        case lex_kind::kOpenParenthesis: {
          advance();
          auto expr = parse_expression();
          if (!expr) return expr;
          if (!match(lex_kind::kCloseParenthesis)) return make_error("Expected ')'");
          return expr;
        }
        default:
          return make_error("Unexpected token in expression");
      }
    }

    constexpr auto parse_func_call(std::string_view name) -> parse_result {
      if (!match(lex_kind::kOpenParenthesis)) return make_error("Expected '('");
      auto args = parse_argument_list();
      if (!args) return std::unexpected{args.error()};
      if (!match(lex_kind::kCloseParenthesis)) return make_error("Expected ')' after arguments");
      return m_pool.add(expr_call{name, *args});
    }

    constexpr auto parse_argument_list() -> parse_result {
      if (auto tok = peek(); !tok || tok->kind == lex_kind::kCloseParenthesis) return empty_node;

      auto first = parse_expression();
      if (!first) return std::unexpected{first.error()};

      index_t head = *first;
      index_t tail = head;
      while (match(lex_kind::kComma)) {
        auto next = parse_expression();
        if (!next) return std::unexpected{next.error()};
        m_pool.nodes[tail].next = *next;
        tail = *next;
      }
      return head;
    }

    // --- Helpers (peek, match, etc.) ---
    constexpr auto is_at_end(std::int32_t offset = 0) const -> bool {
      return m_current + offset >= m_tokens.size();
    }
    constexpr auto peek(std::int32_t offset = 0) -> std::optional<lex_token> {
      if (is_at_end(offset)) return std::nullopt;
      return m_tokens[m_current + offset];
    }
    constexpr auto peek_next() -> std::optional<lex_token> { return peek(1); }
    constexpr auto advance() -> std::optional<lex_token> {
      if (is_at_end()) return std::nullopt;
      return m_tokens[m_current++];
    }
    constexpr auto match(lex_kind kind) -> std::optional<lex_token> {
      auto token = peek();
      if (token && token->kind == kind) { advance(); return token; }
      return std::nullopt;
    }
    constexpr auto make_error(std::string_view message) -> std::unexpected<error_t> {
      return std::unexpected{error_t{error::other_parser_error{{peek()}, message}}};
    }
  };

  template<auto &&tokens>
  constexpr auto parse_tokens() {
    constexpr static auto p = []constexpr{
      return parser{std::span{tokens}}.parse();
    };

    constexpr static auto pp = [] constexpr {
      if constexpr (p()) {
        return std::make_pair(to_array<[]{return p()->first;}>(), p()->second);
      } else {
        constexpr static auto get_error = [] constexpr {
          return p().error();
        };
        report_error<get_error>();
        return p().error();
      }
    };

    return pp();
  }

  template<const_string code>
  constexpr auto parse() {
    constexpr static auto tokens = lex<code>();
    return parse_tokens<tokens>();
  }
}