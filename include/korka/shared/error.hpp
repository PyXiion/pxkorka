#pragma once

#include <variant>
#include "korka/compiler/lex_token.hpp"
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

    struct redeclaration {
      std::string_view identifier;
    };

    constexpr auto report(const redeclaration &err) -> std::string {
      return korka::format("Compiler Error: ~ was redeclared", err.identifier);
    }

    struct unknown_type {
      std::string_view identifier;
    };

    constexpr auto report(const unknown_type &err) -> std::string {
      return korka::format("Compiler Error: unknown type `~`", err.identifier);
    }

    struct undefined_symbol {
      std::string_view identifier;
    };

    constexpr auto report(const undefined_symbol &err) -> std::string {
      return korka::format("Compiler Error: symbol `~` not defined", err.identifier);
    }

    struct function_return_type_mismatch {
      std::string_view return_type;
      std::string_view actual_type;
    };

    constexpr auto report(const function_return_type_mismatch &err) -> std::string {
      return korka::format("Compiler Error: expected ~ type to be returned, got ~", err.return_type, err.actual_type);
    }


    struct other_compiler_error {
      std::string_view message;
    };

    constexpr auto report(const other_compiler_error &err) -> std::string {
      return korka::format("Compiler Error: ~", err.message);
    }

    struct other_error {
      std::string_view message;
    };

    constexpr auto report(const other_error &err) -> std::string {
      return korka::format("Error: ~", err.message);
    }
  }

  using error_t = std::variant<
    error::unexpected_character,
    error::other_lexer_error,
    error::other_parser_error,
    error::redeclaration,
    error::undefined_symbol,
    error::unknown_type,
    error::other_compiler_error,
    error::other_error>;

  constexpr auto to_string(const error_t &err) -> std::string {
    return std::visit([](const auto &e) {
      return error::report(e);
    }, err);
  }

  template<const_string Msg>
  struct ErrorMessage {
    static_assert(false, "Check the template parameter for details");
  };

  template<auto err_getter>
  consteval auto report_error() -> void {
    // Idk, __cpp_static_assert check is not enough for clang
    #if __cplusplus >= 202400L && __cpp_static_assert >= 202306L
    static_assert(false, to_string(err_getter()));
    #else
    constexpr auto msg = const_string_from_string_view<[]{return to_string(err_getter());}>();
    std::ignore = ErrorMessage<msg>{};
    #endif
  }
}