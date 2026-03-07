#include "korka/compiler/parser.hpp"
#include "korka/compiler/compiler.hpp"
#include "korka/compiler/ast_walker.hpp"
#include <print>

constexpr char code[] = R"(
int main() {
  int a = 2;
  if (a) {
    return a;
  } else {
    return 5 + a;
  }
}

int foo(int a, int b) {
  return a + b;
}
)";

constexpr auto compile_result = korka::compile<code>();

auto main_func = compile_result.function<"main">();
static_assert(std::is_same_v<decltype(main_func), long (*)()>);

auto foo_func = compile_result.function<"foo">();
static_assert(std::is_same_v<decltype(foo_func), long (*)(long, long)>);

int main() {
  std::println("{::X}", compile_result.bytes | std::views::transform([](auto b) { return static_cast<int>(b); }));

//  auto lexed = korka::lexer{code}.lex();
//  if (not lexed) {
//    std::println("{}", korka::to_string(lexed.error()));
//    return 0;
//  }
//
//  auto parsed = korka::parser{lexed.value()}.parse();
//  if (not parsed) {
//    std::println("{}", korka::to_string(parsed.error()));
//    return 0;
//  }
//  auto [node_pool, node_root] = parsed.value();
//  std::println("{}", korka::ast_walker{node_pool, node_root, 0});
//
//  korka::compiler compiler{node_pool, node_root};
//  auto bytes = compiler.compile();
//
//  if (bytes) {
//    std::println("{::X}", *bytes | std::views::transform([](auto b) { return static_cast<int>(b); }));
//  } else {
//    std::println("{}", korka::to_string(bytes.error()));
//  }

}