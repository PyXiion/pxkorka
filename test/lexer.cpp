#include "korka/lexer.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("lex_token: Equality operator", "[lexer][unit]") {
  using namespace korka;

  SECTION("Identical tokens") {
    lex_token t1{lex_kind::kNumberLiteral, "123", int64_t{123}, 1};
    lex_token t2{lex_kind::kNumberLiteral, "123", int64_t{123}, 1};
    CHECK(t1 == t2);
  }

  SECTION("Different kinds") {
    lex_token t1{lex_kind::kInt, "int", {}, 1};
    lex_token t2{lex_kind::kIdentifier, "int", {}, 1};
    CHECK(t1 != t2);
  }

  SECTION("Different values in variant") {
    lex_token t1{lex_kind::kNumberLiteral, "10", int64_t{10}, 1};
    lex_token t2{lex_kind::kNumberLiteral, "10", 10.0, 1}; // int vs double
    CHECK(t1 != t2);
  }

  SECTION("Different lines") {
    lex_token t1{lex_kind::kSemicolon, ";", {}, 1};
    lex_token t2{lex_kind::kSemicolon, ";", {}, 2};
    CHECK(t1 != t2);
  }
}

TEST_CASE("Basic lexing", "[lexer]") {
  korka::lexer lexer{
    ""
    "int main() {\n"
    "  puts(\"Hello world!\");\n"
    "  return 0;\n"
    "}"
  };

  auto tokens = lexer.lex();
  auto expected_tokens = std::array{
    korka::lex_token{korka::lex_kind::kInt, "int", std::monostate{}, 1},
    korka::lex_token{korka::lex_kind::kIdentifier, "main", std::monostate{}, 1},
    korka::lex_token{korka::lex_kind::kOpenParenthesis, "(", std::monostate{}, 1},
    korka::lex_token{korka::lex_kind::kCloseParenthesis, ")", std::monostate{}, 1},
    korka::lex_token{korka::lex_kind::kOpenBrace, "{", std::monostate{}, 1},

    korka::lex_token{korka::lex_kind::kIdentifier, "puts", std::monostate{}, 2},
    korka::lex_token{korka::lex_kind::kOpenParenthesis, "(", std::monostate{}, 2},
    korka::lex_token{korka::lex_kind::kStringLiteral, "\"Hello world!\"", "Hello world!", 2},
    korka::lex_token{korka::lex_kind::kCloseParenthesis, ")", std::monostate{}, 2},
    korka::lex_token{korka::lex_kind::kSemicolon, ";", std::monostate{}, 2},

    korka::lex_token{korka::lex_kind::kReturn, "return", std::monostate{}, 3},
    korka::lex_token{korka::lex_kind::kNumberLiteral, "0", 0, 3},
    korka::lex_token{korka::lex_kind::kSemicolon, ";", std::monostate{}, 3},

    korka::lex_token{korka::lex_kind::kCloseBrace, "}", std::monostate{}, 4},
    korka::lex_token{korka::lex_kind::kEof, "", std::monostate{}, 4},

  };

  REQUIRE(tokens.size() == expected_tokens.size());

  for (std::size_t i = 0; i < tokens.size(); ++i) {
    INFO("Checking token at index " << i << std::format(" : {} against {}", tokens[i], expected_tokens[i]));
    CHECK(tokens[i] == expected_tokens[i]);
  }
}

TEST_CASE("Numbers", "[lexer]") {
  auto lex = [](std::string_view s) { return korka::lexer{s}.lex(); };

  SECTION("Integers") {
    auto tokens = lex("123 0 456");
    REQUIRE(tokens.size() == 4); // 3 numbers + EOF
    CHECK(std::get<std::int64_t>(tokens[0].value) == 123);
  }

  SECTION("Floating point") {
    auto tokens = lex("3.14");
    CHECK(tokens[0].kind == korka::lex_kind::kNumberLiteral);
    CHECK(std::get<double>(tokens[0].value) == Catch::Approx(3.14));
  }
}