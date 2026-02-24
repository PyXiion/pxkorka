#pragma once

#include <format>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <variant>
#include <expected>
#include "frozen/unordered_map.h"
#include "frozen/string.h"
#include "korka/utils/string.hpp"
#include "lex_token.hpp"
#include "korka/shared.hpp"
#include "korka/utils/utils.hpp"
#include "error.hpp"

namespace korka {
  class lexer {
  public:
    explicit constexpr lexer(std::string_view source) : m_source(source) {}

    constexpr auto lex() -> std::expected<std::vector<lex_token>, error_t> {
      tokens.clear();

      while (not is_at_end()) {
        start = current;
        auto token = scan_token();

        if (not token) continue;

        if (token->has_value()) {
          tokens.emplace_back(**token);
        } else {
          return std::unexpected{token->error()};
        }
      }

      tokens.emplace_back(lex_kind::kEof, "", std::monostate{}, line, 0);

      return tokens;
    }

  private:
    static constexpr frozen::unordered_map<frozen::string, lex_kind, 10> keywords{
      {"int",    lex_kind::kInt},

      {"return", lex_kind::kReturn},
      {"and",    lex_kind::kAnd},
      {"or",     lex_kind::kOr},
      {"if",     lex_kind::kIf},
      {"else",   lex_kind::kElse},
      {"true",   lex_kind::kTrue},
      {"false",  lex_kind::kFalse},
      {"for",    lex_kind::kFor},
      {"while",  lex_kind::kWhile},
    };

    std::string_view m_source;

    std::vector<lex_token> tokens{};
    std::size_t start{};
    std::size_t current{};
    std::size_t line = 1;
    std::size_t in_line_pos = 0;

    constexpr auto is_at_end() -> bool {
      return current >= m_source.length();
    }

    constexpr auto scan_token() -> std::optional<std::expected<lex_token, error_t>> {
      char c = advance();
      switch (c) {
        case '{':
          return make_token(lex_kind::kOpenBrace);
        case '}':
          return make_token(lex_kind::kCloseBrace);
        case '(':
          return make_token(lex_kind::kOpenParenthesis);
        case ')':
          return make_token(lex_kind::kCloseParenthesis);
        case ';':
          return make_token(lex_kind::kSemicolon);
        case ',':
          return make_token(lex_kind::kComma);
        case '+':
          return make_token(match('=') ? lex_kind::kPlusEqual : lex_kind::kPlus);
        case '-':
          return make_token(match('=') ? lex_kind::kMinusEqual : lex_kind::kMinus);
        case '*':
          return make_token(match('=') ? lex_kind::kStarEqual : lex_kind::kStar);
        case '%':
          return make_token(match('=') ? lex_kind::kPercentEqual : lex_kind::kPercent);
        case '=':
          return make_token(match('=') ? lex_kind::kEqualEqual : lex_kind::kEqual);
        case '<':
          return make_token(match('=') ? lex_kind::kLessEqual : lex_kind::kLess);
        case '>':
          return make_token(match('=') ? lex_kind::kGreaterEqual : lex_kind::kGreater);
        case '/':
          if (match('/')) {
            // Comment until the end of line
            while (peek() != '\n' and not is_at_end()) advance();
          } else {
            return make_token(match('=') ? lex_kind::kSlashEqual : lex_kind::kSlash);
          }
          break;

        case ' ':
        case '\r':
        case '\t':
          // Ignore whitespace
          return std::nullopt;

        case '\n':
          next_line();
          return std::nullopt;

        case '"':
          return scan_string();

        default:
          if (is_digit(c)) {
            return scan_number();
          } else if (is_alpha(c)) {
            return scan_identifier();
          }
      }

      return std::unexpected{
        error::unexpected_character{
          .ctx = {
            .line = line
          },
          .c = c
        }
      };
    }

    constexpr auto scan_string() -> std::expected<lex_token, error_t> {
      while (peek() != '"' and not is_at_end()) {
        if (peek() == '\n') line += 1;
        advance();
      }

      if (is_at_end()) {
        // Error, unterminated string
        return std::unexpected{
          error::other_lexer_error{
            .ctx = {
              .line = line
            },
            .message = "Unterminated string",
          }
        };
      }

      // Eat closing "
      advance();

      auto value = m_source.substr(start + 1, current - start - 2);
      return make_token(lex_kind::kStringLiteral, value);
    }

    constexpr auto scan_number() -> lex_token {
      while (is_digit(peek())) advance();

      bool fp = false;
      if (peek() == '.' and is_digit(peek_next())) {
        fp = true;
        advance();
        while (is_digit(peek())) advance();
      }

      auto value = m_source.substr(start, current - start);
      if (fp) {
        return make_token(lex_kind::kNumberLiteral, to_double(value));
      } else {
        return make_token(lex_kind::kNumberLiteral, to_integer(value));
      }
    }

    constexpr auto scan_identifier() -> lex_token {
      while (is_alphanum(peek())) advance();

      std::string_view text = m_source.substr(start, current - start);
      auto type_it = keywords.find(frozen::string{text});
      lex_kind type = lex_kind::kIdentifier;
      if (type_it != keywords.end()) {
        type = type_it->second;
      }

      return make_token(type);
    }

    constexpr auto advance() -> char {
      in_line_pos += 1;
      return m_source.at(current++);
    }

    constexpr auto match(char expected) -> bool {
      if (is_at_end()) return false;
      if (m_source.at(current) != expected) return false;

      current += 1;
      return true;
    }

    constexpr auto peek() -> char {
      if (is_at_end()) return 0;
      return m_source.at(current);
    }

    constexpr auto peek_next() -> char {
      if (current + 1 >= m_source.length()) return 0;
      return m_source.at(current + 1);
    }

    constexpr auto make_token(lex_kind kind, lex_value &&value = {}) -> lex_token {
      return {kind, m_source.substr(start, current - start), std::move(value), line, in_line_pos};
    }

    constexpr auto next_line() -> void {
      line += 1;
      in_line_pos = 0;
    }

    static constexpr auto is_digit(char c) -> bool {
      return c >= '0' and c <= '9';
    }

    static constexpr auto is_alpha(char c) -> bool {
      return (c >= 'a' and c <= 'z')
             or (c >= 'A' and c <= 'Z')
             or c == '_';
    }

    static constexpr auto is_alphanum(char c) -> bool {
      return is_digit(c) or is_alpha(c);
    }

    static constexpr auto to_integer(std::string_view sv) -> std::int64_t {
      std::int64_t r{};
      for (auto &&c: sv) {
        r *= 10;
        r += c - '0';
      }
      return r;
    }

    static constexpr auto to_double(std::string_view sv) -> double {
      double r{};
      double factor = 1.0;
      bool decimal{};

      for (auto &&c: sv) {
        if (c == '.') {
          decimal = true;
          continue;
        }
        if (decimal) {
          factor /= 10.0;
          r += (c - '0') * factor;
        } else {
          r = r * 10.0 + (c - '0');
        }
      }
      return r;
    }
  };

  template<const_string str>
  consteval auto lex() {
    constexpr static auto expr = [] constexpr {
      return lexer{static_cast<std::string_view>(str)}.lex();
    };

    if constexpr (expr()) {
      constexpr static auto expr_getter = [] constexpr {
        return expr().value();
      };
      return to_array<expr_getter>();
    } else {
      report_error<[] { return expr().error(); }>();
      return expr().error();
    }
  }
} // korka


constexpr auto operator==(const korka::lex_token &l, const korka::lex_token &r) -> bool {
  return l.kind == r.kind
         and l.lexeme == r.lexeme
         and l.value == r.value
         and l.line == r.line;
}

template<>
struct std::formatter<korka::lex_token> : std::formatter<std::string_view> {
  template<class FmtContext>
  auto format(const korka::lex_token &obj, FmtContext &ctx) const -> FmtContext::iterator {
    auto value = std::visit([](const auto &v) -> std::string {
      using type = std::remove_reference_t<decltype(v)>;
      if constexpr (std::convertible_to<type, std::monostate>)
        return "N/D";
      else if constexpr (std::is_arithmetic_v<type>)
        return std::to_string(v);
      else return std::string{v};
    }, obj.value);
    return std::format_to(ctx.out(), "{{{}, kind: {}, value: {}, line: {}}}", obj.lexeme, static_cast<int>(obj.kind),
                          value, obj.line);
  }
};