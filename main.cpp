#include "korka/compiler/lexer.hpp"
#include "korka/compiler/parser.hpp"
#include "korka/compiler/ast_walker.hpp"
#include <print>


constexpr char code[] = R"(
int main() {
  int a;
  a = 5 + 5 * (2 + 2);
}
)";
constexpr auto tokens = korka::lex<code>();

int main() {
  auto parser = korka::parser{tokens};
  auto ast = parser.parse();
  std::println("{}", korka::ast_walker{ast.first, ast.second, 0});
}