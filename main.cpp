#include "korka/compiler/lexer.hpp"
#include "korka/compiler/parser.hpp"
#include <print>


//constexpr char code[] = R"(
//int main() {
//  puts("Hello world!");
//}
//)";
constexpr char code[] = R"(
int main() {}
)";
constexpr auto tokens = korka::lex<code>();



int main() {
//  for (auto &&tk: tokens) {
//    std::println("{}", tk);
//  }

  auto ast = korka::parse_tokens<tokens>();

  std::print("{}", ast.first);
}