#include "korka/compiler/lexer.hpp"
#include <print>


constexpr char code[] = R"(
int main() {
  puts("Hello world!");
}
)";
constexpr auto tokens = korka::lex<code>();

int main() {
  for (auto &&tk: tokens) {
    std::println("{}", tk);
  }
}