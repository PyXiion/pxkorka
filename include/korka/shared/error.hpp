#pragma once

#include <concepts>
#include <variant>
#include "korka/utils/const_format.hpp"
#include <tuple>
#include "korka/utils/string.hpp"
#include <string>
#include <string_view>

#if __cplusplus >= 202400L && __cpp_static_assert >= 202306L
#define KORKA_FEATURE_FORMATTED_STATIC_ASSERT
#endif

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
      std::string_view lexeme;
      std::size_t line;
      std::size_t char_pos;
    };

    struct other_parser_error {
      parser_context ctx;
      std::string_view message;
    };

    constexpr auto report(const other_parser_error &err) -> std::string {
      return korka::format("Parser Error: ~ at ~:~ (token: ~)", err.message, err.ctx.line, err.ctx.char_pos,
                           err.ctx.lexeme);
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

    struct type_mismatch {
      std::string_view context; // "initializer", "assignment", "argument", "return value", "condition", "operand"
      std::string_view subject; // variable or function the value is used for; may be empty
      std::string_view expected;
      std::string_view actual;
    };

    constexpr auto report(const type_mismatch &err) -> std::string {
      if (err.subject.empty()) {
        return korka::format("Compiler Error: type mismatch in ~: expected '~', got '~'",
                             err.context, err.expected, err.actual);
      }
      return korka::format("Compiler Error: type mismatch in ~ of '~': expected '~', got '~'",
                           err.context, err.subject, err.expected, err.actual);
    }

    struct argument_count_mismatch {
      std::string_view function_name;
      std::size_t expected;
      std::size_t actual;
    };

    constexpr auto report(const argument_count_mismatch &err) -> std::string {
      return korka::format("Compiler Error: function '~' takes ~ argument(s), got ~",
                           err.function_name, err.expected, err.actual);
    }

    struct unknown_field {
      std::string_view struct_name;
      std::string_view field_name;
    };

    constexpr auto report(const unknown_field &err) -> std::string {
      return korka::format("Compiler Error: struct '~' has no field '~'", err.struct_name, err.field_name);
    }

    struct member_on_non_struct {
      std::string_view type_name;
    };

    constexpr auto report(const member_on_non_struct &err) -> std::string {
      return korka::format("Compiler Error: member access on a value of non-struct type '~'", err.type_name);
    }

    struct not_assignable {
    };

    constexpr auto report(const not_assignable &) -> std::string {
      return "Compiler Error: the left side of an assignment must be a variable or a member of a variable";
    }

    struct missing_return {
      std::string_view function_name;
    };

    constexpr auto report(const missing_return &err) -> std::string {
      return korka::format("Compiler Error: not every path of function '~' returns a value", err.function_name);
    }

    struct other_compiler_error {
      std::string_view message;
    };

    constexpr auto report(const other_compiler_error &err) -> std::string {
      return korka::format("Compiler Error: ~", err.message);
    }
  }

  using error_t = std::variant<
    error::unexpected_character,
    error::other_lexer_error,
    error::other_parser_error,
    error::redeclaration,
    error::undefined_symbol,
    error::unknown_type,
    error::type_mismatch,
    error::argument_count_mismatch,
    error::unknown_field,
    error::member_on_non_struct,
    error::not_assignable,
    error::missing_return,
    error::other_compiler_error>;

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
    if constexpr (std::convertible_to<decltype(err_getter()), error_t>) {
      #ifdef KORKA_FEATURE_FORMATTED_STATIC_ASSERT
      static_assert(false, to_string(err_getter()));
      #else
      constexpr auto msg = const_string_from_string_view<[] { return to_string(err_getter()); }>();
      std::ignore = ErrorMessage<msg>{};
      #endif
    } else {
      #ifdef KORKA_FEATURE_FORMATTED_STATIC_ASSERT
      static_assert(false, err_getter());
      #else
      constexpr auto msg = const_string_from_string_view<err_getter>();
      std::ignore = ErrorMessage<msg>{};
      #endif
    }
  }

  template<bool ok, auto err_getter>
  consteval auto format_static_assert() -> void {
    if constexpr (ok) {
      // yay
    } else {
      report_error<err_getter>();
    }
  }
}
