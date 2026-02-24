#pragma once
#include <string_view>

namespace korka {
  // @formatter:off
  enum struct lex_kind {
    kOpenBrace,         // {
    kCloseBrace,        // }
    kOpenParenthesis,   // (
    kCloseParenthesis,  // )
    kSemicolon,         // ;
    kComma,             // ,

    kBang,    kBangEqual,     // !, !=
    kEqual,   kEqualEqual,    // =, ==
    kLess,    kLessEqual,     // <, <=
    kGreater, kGreaterEqual,  // >, >=

    kPlus,    kPlusEqual,     // +, +=
    kMinus,   kMinusEqual,    // -, -=
    kSlash,   kSlashEqual,    // /, /=
    kPercent, kPercentEqual,  // %, %=
    kStar,    kStarEqual,     // *, *=


    kInt,               // int

    kReturn,            // return
    kAnd, kOr,          // and, or
    kIf, kElse,         // if, else
    kTrue, kFalse,      // true, false
    kFor, kWhile,       // for, while


    kIdentifier,        // [a-zA-Z]\w*
    kStringLiteral,     // "[^"]+"
    kNumberLiteral,    // [0-9]+

    kEof,
  };
  // @formatter:on

  using lex_value = literal_value_t;

  struct lex_token {
    constexpr lex_token()
      : kind(lex_kind::kEof), lexeme(), value(std::monostate{}),
        line(0) {}

    constexpr lex_token(lex_kind kind_, const std::string_view &lexeme_, lex_value value_, size_t line_, size_t char_pos_)
      : kind(kind_), lexeme(lexeme_), value(value_),
        line(line_), char_pos(char_pos_) {}

    lex_kind kind;
    std::string_view lexeme{};
    lex_value value{};
    std::size_t line{};
    std::size_t char_pos{};
  };
}