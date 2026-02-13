#include "korka/compiler/lexer.hpp"
#include "korka/compiler/parser.hpp"
#include "korka/compiler/ast_walker.hpp"
#include <print>


constexpr char code[] = R"(
int main() {
  int a;
  int b;
  int c;
}
)";
constexpr auto tokens = korka::lex<code>();

int main() {
  auto ast = korka::parse_tokens<tokens>();
  std::print("{}", korka::ast_walker{ast.first, ast.second, 0});
}