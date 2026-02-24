#pragma once

#include <variant>
#include "lex_token.hpp"
#include "korka/utils/const_format.hpp"
#include <optional>

namespace korka {
  namespace error {
    struct lexer_context {
      std::size_t line;
    };

    struct unexpected_character {
      lexer_context ctx;
      char c;
    };

    constexpr auto report(const unexpected_character &err) -> std::string {
      return format("Lexer Error: Unexpected character '~' at line ~", err.c, err.ctx.line);
    }

    struct other_lexer_error {
      lexer_context ctx;
      std::string_view message;
    };

    constexpr auto report(const other_lexer_error &err) -> std::string {
      return korka::format("Lexer Error: ~ at line ~", err.message, err.ctx.line);
    }


    struct parser_context {
      std::optional<lex_token> lexeme;
    };

    struct other_parser_error {
      parser_context ctx;
      std::string_view message;
    };

    constexpr auto report(const other_parser_error &err) -> std::string {
      if (err.ctx.lexeme) {
        auto &l = err.ctx.lexeme;
        return korka::format("Parser Error: ~ at ~:~ (token: ~)", err.message, l->line, l->char_pos, l->lexeme);
      }
      return korka::format("Parser Error: ~:~", err.message, err.ctx.lexeme->char_pos);
    }
  }

  using error_t = std::variant<
    error::unexpected_character,
    error::other_lexer_error,
    error::other_parser_error>;

  constexpr auto to_string(const error_t &err) -> std::string {
    return std::visit([](const auto &e) {
      return error::report(e);
    }, err);
  }

  template<auto err_getter>
  consteval auto report_error() -> void {
    static_assert(false, to_string(err_getter()));
  }
}