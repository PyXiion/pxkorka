#include "korka/compiler/lexer.hpp"
#include "korka/compiler/parser.hpp"
#include "korka/compiler/ast_walker.hpp"
#include <print>


constexpr char code[] = R"(
int main() {
  int a;
  a = 5 + 5 * (2 + 2);
  int b;
}
)";
constexpr auto tokens = korka::lex<code>();
constexpr auto ast = korka::parse_tokens<tokens>();

int main() {
  std::println("{}", korka::ast_walker{ast.first, ast.second, 0});
}