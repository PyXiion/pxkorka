#include <catch_amalgamated.hpp>
#include "korka/compiler/lexer.hpp"
#include "korka/compiler/parser.hpp"
#include <string>
#include <vector>
#include <span>
#include <expected>
#include <variant>

using namespace korka;
auto StrContains(const std::string &str) {
  return Catch::Matchers::StringContainsMatcher({str, Catch::CaseSensitive::No});
};

static auto parse_code(std::string_view code) -> std::pair<std::vector<parser::node>, parser::index_t> {
  auto tokens = lexer{code}.lex();
  REQUIRE(tokens);
  parser p(tokens.value());
  auto result = p.parse();
  if (!result) {
    FAIL(to_string(result.error()));
  }
  return std::move(result).value();
}

// Node access helpers
template<typename T>
const T &get_node_as(const parser::node &n) {
  return std::get<T>(n.data);
}

TEST_CASE("Parser accepts empty program", "[parser]") {
  auto [nodes, root] = parse_code("");
  REQUIRE(root != parser::empty_node);
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  CHECK(prog.external_declarations_head == parser::empty_node);
}

TEST_CASE("Parser accepts function with no parameters and empty body", "[parser][function]") {
  auto [nodes, root] = parse_code("int main() {}");
  REQUIRE(root != parser::empty_node);
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  REQUIRE(prog.external_declarations_head != parser::empty_node);

  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  CHECK(func.ret_type == "int");
  CHECK(func.name == "main");
  CHECK(func.params_head == parser::empty_node);
  CHECK(func.body != parser::empty_node);

  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);
  CHECK(body.children_head == parser::empty_node);
}

TEST_CASE("Parser accepts function with parameters", "[parser][function]") {
  auto [nodes, root] = parse_code("int sum(int a, int b) { return a+b; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);

// Check parameters list
  REQUIRE(func.params_head != parser::empty_node);
  const auto &param1 = get_node_as<parser::decl_var>(nodes[func.params_head]);
  CHECK(param1.type_name == "int");
  CHECK(param1.var_name == "a");
  REQUIRE(nodes[func.params_head].next != parser::empty_node);
  const auto &param2 = get_node_as<parser::decl_var>(nodes[nodes[func.params_head].next]);
  CHECK(param2.type_name == "int");
  CHECK(param2.var_name == "b");
  CHECK(nodes[nodes[func.params_head].next].next == parser::empty_node);

// Check body StringContainsMatcher return statement
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);
  REQUIRE(body.children_head != parser::empty_node);
  const auto &ret = get_node_as<parser::stmt_return>(nodes[body.children_head]);
  CHECK(ret.expr != parser::empty_node);
}

TEST_CASE("Parser accepts local variable declaration", "[parser][declaration]") {
  auto [nodes, root] = parse_code("void foo() { int x = 5; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);

  REQUIRE(body.children_head != parser::empty_node);
  const auto &decl = get_node_as<parser::decl_var>(nodes[body.children_head]);
  CHECK(decl.type_name == "int");
  CHECK(decl.var_name == "x");
  REQUIRE(decl.init_expr != parser::empty_node);
  const auto &lit = get_node_as<parser::expr_literal>(nodes[decl.init_expr]);
  CHECK(std::holds_alternative<int64_t>(lit));
  CHECK(std::get<int64_t>(lit) == 5);
}

TEST_CASE("Parser accepts if statement", "[parser][stmt]") {
  auto [nodes, root] = parse_code("void test() { if (x) y = 1; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);

  REQUIRE(body.children_head != parser::empty_node);
  const auto &stmt = get_node_as<parser::stmt_if>(nodes[body.children_head]);
  CHECK(stmt.condition != parser::empty_node);
  CHECK(stmt.then_branch != parser::empty_node);
  CHECK(stmt.else_branch == parser::empty_node);
}

TEST_CASE("Parser accepts if-else statement", "[parser][stmt]") {
  auto [nodes, root] = parse_code("void test() { if (x) y = 1; else y = 2; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);

  REQUIRE(body.children_head != parser::empty_node);
  const auto &stmt = get_node_as<parser::stmt_if>(nodes[body.children_head]);
  CHECK(stmt.condition != parser::empty_node);
  CHECK(stmt.then_branch != parser::empty_node);
  CHECK(stmt.else_branch != parser::empty_node);
}

TEST_CASE("Parser accepts while statement", "[parser][stmt]") {
  auto [nodes, root] = parse_code("void loop() { while (i < 10) i = i + 1; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);

  REQUIRE(body.children_head != parser::empty_node);
  const auto &stmt = get_node_as<parser::stmt_while>(nodes[body.children_head]);
  CHECK(stmt.condition != parser::empty_node);
  CHECK(stmt.body != parser::empty_node);
}

TEST_CASE("Parser accepts return with expression", "[parser][stmt]") {
  auto [nodes, root] = parse_code("int foo() { return 42; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);

  REQUIRE(body.children_head != parser::empty_node);
  const auto &ret = get_node_as<parser::stmt_return>(nodes[body.children_head]);
  REQUIRE(ret.expr != parser::empty_node);
  const auto &lit = get_node_as<parser::expr_literal>(nodes[ret.expr]);
  CHECK(std::get<int64_t>(lit) == 42);
}

TEST_CASE("Parser accepts return without expression", "[parser][stmt]") {
  auto [nodes, root] = parse_code("void foo() { return; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);

  REQUIRE(body.children_head != parser::empty_node);
  const auto &ret = get_node_as<parser::stmt_return>(nodes[body.children_head]);
  CHECK(ret.expr == parser::empty_node);
}

TEST_CASE("Parser accepts expression statement", "[parser][stmt]") {
  auto [nodes, root] = parse_code("void foo() { x = 5; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);

  REQUIRE(body.children_head != parser::empty_node);
  const auto &expr_stmt = get_node_as<parser::stmt_expr>(nodes[body.children_head]);
  REQUIRE(expr_stmt.expr != parser::empty_node);
// Should be an assignment (binary with op "=")
  const auto &assign = get_node_as<parser::expr_binary>(nodes[expr_stmt.expr]);
  CHECK(assign.op == "=");
}

TEST_CASE("Parser accepts binary operators with correct precedence", "[parser][expr]") {
  auto [nodes, root] = parse_code("int eval() { return a + b * c; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);
  const auto &ret = get_node_as<parser::stmt_return>(nodes[body.children_head]);

// Expression: a + (b * c)
  const auto &add = get_node_as<parser::expr_binary>(nodes[ret.expr]);
  CHECK(add.op == "+");
  const auto &left_var = get_node_as<parser::expr_var>(nodes[add.left]);
  CHECK(left_var.name == "a");
  const auto &mul = get_node_as<parser::expr_binary>(nodes[add.right]);
  CHECK(mul.op == "*");
  const auto &mul_left = get_node_as<parser::expr_var>(nodes[mul.left]);
  CHECK(mul_left.name == "b");
  const auto &mul_right = get_node_as<parser::expr_var>(nodes[mul.right]);
  CHECK(mul_right.name == "c");
}

TEST_CASE("Parser accepts assignment expression", "[parser][expr]") {
  auto [nodes, root] = parse_code("void foo() { x = y = 5; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);
  const auto &expr_stmt = get_node_as<parser::stmt_expr>(nodes[body.children_head]);

// Expression: x = (y = 5)
  const auto &assign1 = get_node_as<parser::expr_binary>(nodes[expr_stmt.expr]);
  CHECK(assign1.op == "=");
  const auto &var_x = get_node_as<parser::expr_var>(nodes[assign1.left]);
  CHECK(var_x.name == "x");
  const auto &assign2 = get_node_as<parser::expr_binary>(nodes[assign1.right]);
  CHECK(assign2.op == "=");
  const auto &var_y = get_node_as<parser::expr_var>(nodes[assign2.left]);
  CHECK(var_y.name == "y");
  const auto &lit = get_node_as<parser::expr_literal>(nodes[assign2.right]);
  CHECK(std::get<int64_t>(lit) == 5);
}

TEST_CASE("Parser accepts function call", "[parser][expr]") {
  auto [nodes, root] = parse_code("int foo() { return bar(1, 2); }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);
  const auto &ret = get_node_as<parser::stmt_return>(nodes[body.children_head]);

  const auto &call = get_node_as<parser::expr_call>(nodes[ret.expr]);
  CHECK(call.name == "bar");
  REQUIRE(call.args_head != parser::empty_node);

// First argument: 1
  const auto &arg1 = get_node_as<parser::expr_literal>(nodes[call.args_head]);
  CHECK(std::get<int64_t>(arg1) == 1);
  REQUIRE(nodes[call.args_head].next != parser::empty_node);

// Second argument: 2
  const auto &arg2 = get_node_as<parser::expr_literal>(nodes[nodes[call.args_head].next]);
  CHECK(std::get<int64_t>(arg2) == 2);
  CHECK(nodes[nodes[call.args_head].next].next == parser::empty_node);
}

TEST_CASE("Parser accepts unary operators", "[parser][expr]") {
  auto [nodes, root] = parse_code("int foo() { return -x; }");
  const auto &prog = get_node_as<parser::decl_program>(nodes[root]);
  const auto &func = get_node_as<parser::decl_function>(nodes[prog.external_declarations_head]);
  const auto &body = get_node_as<parser::stmt_block>(nodes[func.body]);
  const auto &ret = get_node_as<parser::stmt_return>(nodes[body.children_head]);

  const auto &unary = get_node_as<parser::expr_unary>(nodes[ret.expr]);
  CHECK(unary.op == "-");
  const auto &var = get_node_as<parser::expr_var>(nodes[unary.child]);
  CHECK(var.name == "x");
}

TEST_CASE("Parser rejects missing semicolon", "[parser][error]") {
  auto tokens = lexer{"int foo() { return 42 }"}.lex(); // missing ;
  REQUIRE(tokens);
  parser p(*tokens);
  auto result = p.parse();
  REQUIRE_FALSE(result.has_value());
  CHECK_THAT(to_string(result.error()), StrContains("Expected ';'"));
}

TEST_CASE("Parser rejects missing closing brace", "[parser][error]") {
  auto tokens = lexer{"int foo() { return 42; "}.lex(); // missing }
  REQUIRE(tokens);
  parser p(*tokens);
  auto result = p.parse();
  REQUIRE_FALSE(result.has_value());
  CHECK_THAT(to_string(result.error()), StrContains("Expected '}'"));
}

TEST_CASE("Parser rejects missing parentheses in if", "[parser][error]") {
  auto tokens = lexer{"void foo() { if x ) {} }"}.lex(); // missing '('
  REQUIRE(tokens);
  parser p(*tokens);
  auto result = p.parse();
  REQUIRE_FALSE(result.has_value());
  CHECK_THAT(to_string(result.error()), StrContains("Expected '('"));
}

TEST_CASE("Parser rejects invalid expression", "[parser][error]") {
  auto tokens = lexer{"int foo() { return 5 + ; }"}.lex(); // missing right operand
  REQUIRE(tokens);
  parser p(*tokens);
  auto result = p.parse();
  REQUIRE_FALSE(result.has_value());
  // The exact error might be "Expected expression" or similar
  CHECK(to_string(result.error()).find("expression") != std::string::npos);
}

TEST_CASE("Parser rejects variable declaration without type", "[parser][error]") {
  auto tokens = lexer{"foo() { int x; }"}.lex(); // missing return type
  REQUIRE(tokens);
  parser p(*tokens);
  auto result = p.parse();
  REQUIRE_FALSE(result.has_value());
}