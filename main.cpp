#include "include/korka/lexer.hpp"
#include <ranges>
#include <print>

constexpr char code[] = R"(
int main() {
  puts("Hello world!");
}
)";
constexpr auto tokens = korka::lex<code>();

int main() {
  std::println("{}", tokens);
}