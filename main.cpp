#include "korka/compiler/parser.hpp"
#include "korka/compiler/ast_walker.hpp"
#include <print>

constexpr char code[] = R"(
int main() {
  int i = 0;
  while(i + foo(i) <= 4500) {
    print("Abc");

    i = 5 + 5 + foo("42 + 55", 2);
  }
  return 0;
}
)";

//constexpr static auto tokens = korka::lex<code>();

constexpr auto ast = korka::parse<code>();

constexpr auto node_pool = ast.first;
constexpr auto node_root = ast.second;

int main() {
  std::println("{}", korka::ast_walker{node_pool, node_root, 0});
}