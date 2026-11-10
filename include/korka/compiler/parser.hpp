#pragma once

#include "korka/shared.hpp"
#include "lexer.hpp"
#include <algorithm>
#include <expected>
#include <optional>
#include <variant>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <ranges>

namespace korka {
  namespace nodes {
    using index_t = int32_t;
    constexpr static index_t empty_node = -1;

    enum class unary_op { neg, plus, not_ };

    enum class binary_op { add, sub, mul, div, mod, eq, ne, lt, le, gt, ge, and_, or_ };

    constexpr auto op_text(unary_op op) -> std::string_view {
      switch (op) {
        case unary_op::neg: return "-";
        case unary_op::plus: return "+";
        case unary_op::not_: return "!";
      }
      return "?";
    }

    constexpr auto op_text(binary_op op) -> std::string_view {
      switch (op) {
        case binary_op::add: return "+";
        case binary_op::sub: return "-";
        case binary_op::mul: return "*";
        case binary_op::div: return "/";
        case binary_op::mod: return "%";
        case binary_op::eq: return "==";
        case binary_op::ne: return "!=";
        case binary_op::lt: return "<";
        case binary_op::le: return "<=";
        case binary_op::gt: return ">";
        case binary_op::ge: return ">=";
        case binary_op::and_: return "and";
        case binary_op::or_: return "or";
      }
      return "?";
    }

    // --- Node Structures ---
    using expr_literal = literal_value_t;
    struct expr_var { std::string_view name; };
    struct expr_unary { unary_op op; index_t child; };
    struct expr_binary { binary_op op; index_t left; index_t right; };
    struct expr_assign { index_t target; index_t value; };
    struct expr_member { index_t object; std::string_view field_name; };
    struct expr_call { std::string_view name; index_t args_head; };
    struct stmt_block { index_t children_head; };
    struct stmt_if { index_t condition; index_t then_branch; index_t else_branch; };
    struct stmt_while { index_t condition; index_t body; };
    struct stmt_return { index_t expr; };
    struct stmt_expr { index_t expr; };
    struct decl_var { std::string_view type_name; std::string_view var_name; index_t init_expr; };
    struct decl_struct { std::string_view name; index_t fields_head; };
    struct decl_function { std::string_view ret_type; std::string_view name; index_t params_head; index_t body; };
    struct decl_program { index_t external_declarations_head; };

    struct node {
      using data_t = std::variant<
        expr_literal, expr_var, expr_unary, expr_binary, expr_assign, expr_member, expr_call,
        stmt_block, stmt_if, stmt_while, stmt_return, stmt_expr, decl_var,
        decl_struct, decl_function, decl_program
      >;
      data_t data;
      index_t next = empty_node;
    };


    struct index_iterator {
      using value_type = index_t;
      using difference_type = std::ptrdiff_t;

      index_t current;
      std::span<const node> nodes;

      constexpr auto operator*() const -> index_t { return current; }
      constexpr auto operator++() -> index_iterator& {
        current = nodes[current].next;
        return *this;
      }
      constexpr auto operator++(int) -> index_iterator {
        auto self = *this;
        current = nodes[current].next;
        return self;
      }
      constexpr auto operator==(std::default_sentinel_t) const -> bool {
        return current == empty_node;
      }
    };

    constexpr auto get_list_view(std::span<const node> nodes, index_t head) {
      return std::ranges::subrange(
        index_iterator{head, nodes},
        std::default_sentinel
      );
    }
  }
  using namespace korka::nodes;

  class parser {
  public:
    struct ast_pool {
      std::vector<node> nodes{};

      template<typename T>
      constexpr auto add(T &&data) -> index_t {
        nodes.emplace_back(std::forward<T>(data), empty_node);
        return static_cast<index_t>(nodes.size() - 1);
      }
    };

    // Appends to a node list in O(1)
    struct list_builder {
      index_t head = empty_node;
      index_t tail = empty_node;

      constexpr auto append(ast_pool &pool, index_t item) -> void {
        if (head == empty_node) {
          head = item;
        } else {
          pool.nodes[tail].next = item;
        }
        tail = item;
      }
    };

  public:
    using parse_result = std::expected<index_t, error_t>;

    constexpr explicit parser(std::span<const lex_token> tokens) : m_tokens(tokens) {};

    constexpr auto parse() -> std::expected<std::pair<std::vector<node>, index_t>, error_t> {
      list_builder declarations;

      while (peek().kind != lex_kind::kEof) {
        auto decl = parse_external_declaration();
        if (not decl) return std::unexpected{decl.error()};
        declarations.append(m_pool, *decl);
      }

      index_t root = m_pool.add(decl_program{declarations.head});
      return std::make_pair(std::move(m_pool.nodes), root);
    }

  private:
    std::span<const lex_token> m_tokens;
    lex_token m_eof{}; // returned when the token list is empty
    ast_pool m_pool{};
    std::size_t m_current{0};

    constexpr auto parse_external_declaration() -> parse_result {
      if (match(lex_kind::kStruct)) return parse_struct_declaration();

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

      return make_error("Global variables are not supported");
    };

    // `struct` is already consumed
    constexpr auto parse_struct_declaration() -> parse_result {
      auto name = parse_id();
      if (not name) return std::unexpected{name.error()};

      if (!match(lex_kind::kOpenBrace)) return make_error("Expected '{' after the struct name");

      list_builder fields;
      while (not check(lex_kind::kCloseBrace)) {
        auto type = parse_type_specifier();
        if (not type) return std::unexpected{type.error()};

        auto field_name = parse_id();
        if (not field_name) return std::unexpected{field_name.error()};

        if (!match(lex_kind::kSemicolon)) return make_error("Expected ';' after a struct field");

        fields.append(m_pool, m_pool.add(decl_var{*type, *field_name, empty_node}));
      }

      if (fields.head == empty_node) return make_error("A struct needs at least one field");

      advance(); // '}'
      if (!match(lex_kind::kSemicolon)) return make_error("Expected ';' after a struct declaration");

      return m_pool.add(decl_struct{*name, fields.head});
    }

    constexpr auto parse_parameter_list() -> parse_result {
      if (check(lex_kind::kCloseParenthesis)) {
        return empty_node;
      }

      list_builder params;
      do {
        auto param = parse_parameter_decl();
        if (!param) return std::unexpected{param.error()};
        params.append(m_pool, *param);
      } while (match(lex_kind::kComma));

      return params.head;
    }

    constexpr auto parse_parameter_decl() -> parse_result {
      auto type = parse_type_specifier();
      if (not type) return std::unexpected{type.error()};

      auto name = parse_id();
      if (!name) return std::unexpected{name.error()};

      return m_pool.add(decl_var{*type, *name, empty_node});
    }

    constexpr auto parse_type_specifier() -> std::expected<std::string_view, error_t> {
      const auto &token = peek();
      if (token.kind != lex_kind::kInt and token.kind != lex_kind::kVoid and token.kind != lex_kind::kIdentifier) {
        return make_error("Expected builtin type or type identifier");
      }
      advance();
      return token.lexeme;
    }

    constexpr auto parse_id() -> std::expected<std::string_view, error_t> {
      const auto &token = peek();
      if (token.kind != lex_kind::kIdentifier) {
        return make_error("Expected identifier");
      }
      advance();
      return token.lexeme;
    }

    // A declaration is a type followed by a name: `int a`, `void a`, `Vec a`
    constexpr auto starts_declaration() const -> bool {
      const auto kind = peek().kind;
      return (kind == lex_kind::kInt or kind == lex_kind::kVoid or kind == lex_kind::kIdentifier)
             and peek(1).kind == lex_kind::kIdentifier;
    }

    constexpr auto parse_declaration() -> parse_result {
      auto type = parse_type_specifier();
      if (!type) return std::unexpected{type.error()};

      auto name = parse_id();
      if (!name) return std::unexpected{name.error()};

      index_t init_expr = empty_node;
      if (match(lex_kind::kEqual)) {
        auto expr = parse_expression();
        if (!expr) return std::unexpected{expr.error()};
        init_expr = *expr;
      }

      if (!match(lex_kind::kSemicolon)) {
        return make_error("Expected ';' after variable declaration");
      }

      return m_pool.add(decl_var{*type, *name, init_expr});
    }

    // Returns `empty_node` for an empty statement
    constexpr auto parse_statement() -> parse_result {
      switch (peek().kind) {
        case lex_kind::kOpenBrace: return parse_compound_stmt();
        case lex_kind::kIf:        return parse_if_statement();
        case lex_kind::kWhile:     return parse_while_statement();
        case lex_kind::kReturn:    return parse_return_statement();
        case lex_kind::kSemicolon:
          advance();
          return empty_node;
        default:                   return parse_expression_stmt();
      }
    }

    constexpr auto parse_return_statement() -> parse_result {
      if (!match(lex_kind::kReturn)) return make_error("Expected 'return'");

      index_t expr_idx = empty_node;
      if (not check(lex_kind::kSemicolon)) {
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

      list_builder items;

      while (not check(lex_kind::kCloseBrace)) {
        if (check(lex_kind::kEof)) return make_error("Expected '}'");

        auto item = starts_declaration() ? parse_declaration() : parse_statement();
        if (!item) return item;

        if (*item != empty_node) items.append(m_pool, *item);
      }

      advance(); // '}'
      return m_pool.add(stmt_block{items.head});
    }

    constexpr auto parse_expression_stmt() -> parse_result {
      auto expr = parse_expression();
      if (!expr) return std::unexpected{expr.error()};

      if (!match(lex_kind::kSemicolon)) return make_error("Expected ';' after expression");
      return m_pool.add(stmt_expr{*expr});
    }

    constexpr auto parse_expression() -> parse_result { return parse_assignment(); }

    constexpr auto parse_assignment() -> parse_result {
      auto left = parse_binary(0);
      if (!left) return left;

      if (match(lex_kind::kEqual)) {
        const auto &target = m_pool.nodes[*left].data;
        if (not std::holds_alternative<expr_var>(target) and not std::holds_alternative<expr_member>(target)) {
          return make_error("The left side of '=' must be a variable or a member");
        }

        auto right = parse_assignment();
        if (!right) return std::unexpected{right.error()};
        return m_pool.add(expr_assign{*left, *right});
      }
      return left;
    }

    // Binary operators by precedence, loosest first
    static constexpr int binary_levels = 6;

    static constexpr auto binary_op_at(lex_kind kind, int level) -> std::optional<binary_op> {
      switch (level) {
        case 0:
          if (kind == lex_kind::kOr) return binary_op::or_;
          break;
        case 1:
          if (kind == lex_kind::kAnd) return binary_op::and_;
          break;
        case 2:
          if (kind == lex_kind::kEqualEqual) return binary_op::eq;
          if (kind == lex_kind::kBangEqual) return binary_op::ne;
          break;
        case 3:
          if (kind == lex_kind::kLess) return binary_op::lt;
          if (kind == lex_kind::kLessEqual) return binary_op::le;
          if (kind == lex_kind::kGreater) return binary_op::gt;
          if (kind == lex_kind::kGreaterEqual) return binary_op::ge;
          break;
        case 4:
          if (kind == lex_kind::kPlus) return binary_op::add;
          if (kind == lex_kind::kMinus) return binary_op::sub;
          break;
        case 5:
          if (kind == lex_kind::kStar) return binary_op::mul;
          if (kind == lex_kind::kSlash) return binary_op::div;
          if (kind == lex_kind::kPercent) return binary_op::mod;
          break;
      }
      return std::nullopt;
    }

    constexpr auto parse_binary(int level) -> parse_result {
      if (level == binary_levels) return parse_unary();

      auto left = parse_binary(level + 1);
      if (!left) return left;
      while (auto op = binary_op_at(peek().kind, level)) {
        advance();
        auto right = parse_binary(level + 1);
        if (!right) return std::unexpected{right.error()};
        left = m_pool.add(expr_binary{*op, *left, *right});
      }
      return left;
    }

    constexpr auto parse_unary() -> parse_result {
      std::optional<unary_op> op;
      switch (peek().kind) {
        case lex_kind::kMinus: op = unary_op::neg; break;
        case lex_kind::kPlus: op = unary_op::plus; break;
        case lex_kind::kBang: op = unary_op::not_; break;
        default: break;
      }

      if (op) {
        advance();
        auto child = parse_unary();
        if (!child) return std::unexpected{child.error()};
        return m_pool.add(expr_unary{*op, *child});
      }
      return parse_postfix();
    }

    // primary { "." identifier }
    constexpr auto parse_postfix() -> parse_result {
      auto expr = parse_primary();
      if (!expr) return expr;

      while (match(lex_kind::kDot)) {
        auto field = parse_id();
        if (!field) return std::unexpected{field.error()};
        expr = m_pool.add(expr_member{*expr, *field});
      }
      return expr;
    }

    constexpr auto parse_primary() -> parse_result {
      const auto &tok = peek();

      switch (tok.kind) {
        case lex_kind::kIdentifier: {
          advance();
          if (check(lex_kind::kOpenParenthesis)) {
            return parse_func_call(tok.lexeme);
          }
          return m_pool.add(expr_var{tok.lexeme});
        }
        case lex_kind::kStringLiteral:
        case lex_kind::kNumberLiteral: {
          advance();
          return m_pool.add(expr_literal{tok.value});
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
      if (check(lex_kind::kCloseParenthesis)) return empty_node;

      list_builder args;
      do {
        auto arg = parse_expression();
        if (!arg) return std::unexpected{arg.error()};
        args.append(m_pool, *arg);
      } while (match(lex_kind::kComma));

      return args.head;
    }

    // --- Helpers (peek, match, etc.) ---

    // The last token is always `kEof`, looking past it returns it again
    constexpr auto peek(std::size_t offset = 0) const -> const lex_token & {
      if (m_tokens.empty()) return m_eof;
      return m_tokens[std::min(m_current + offset, m_tokens.size() - 1)];
    }
    constexpr auto check(lex_kind kind) const -> bool { return peek().kind == kind; }
    constexpr auto advance() -> void {
      if (m_current < m_tokens.size()) ++m_current;
    }
    constexpr auto match(lex_kind kind) -> bool {
      if (not check(kind)) return false;
      advance();
      return true;
    }
    constexpr auto make_error(std::string_view message) const -> std::unexpected<error_t> {
      const auto &token = peek();
      return std::unexpected{error_t{error::other_parser_error{
        {token.lexeme, token.line, token.char_pos}, message
      }}};
    }
  };

  template<auto tokens_getter>
  consteval auto parse_tokens() {
    constexpr static auto expected = []constexpr{
      auto tokens = tokens_getter();
      return parser{std::span{tokens}}.parse();
    };

    if constexpr(not expected()) {
      report_error<[] {return expected().error();}>();
      return expected().error();
    } else {
      // One parse per constexpr variable: every evaluation has its own constexpr step budget
      constexpr static std::size_t node_count = [] { return expected()->first.size(); }();
      constexpr static index_t root = [] { return expected()->second; }();
      constexpr static auto nodes = []() constexpr {
        auto result = expected();
        std::array<node, node_count> out{};
        std::ranges::copy(result->first, out.begin());
        return out;
      }();

      return std::make_pair(nodes, root);
    }
  }

  template<const_string code>
  consteval auto parse() {
    return parse_tokens<[] { return lex<code>(); }>();
  }
}
