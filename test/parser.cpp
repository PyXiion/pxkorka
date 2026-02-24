//
// Created by pyxiion on 14.02.2026.
//
#include <catch_amalgamated.hpp>
#include "korka/compiler/parser.hpp"

using namespace korka;

using index_t = parser::index_t;
using node = parser::node;

using expr_literal = parser::expr_literal;
using expr_var = parser::expr_var;
using expr_unary = parser::expr_unary;
using expr_binary = parser::expr_binary;
using expr_call = parser::expr_call;
using stmt_block = parser::stmt_block;
using stmt_if = parser::stmt_if;
using stmt_while = parser::stmt_while;
using stmt_return = parser::stmt_return;
using stmt_expr = parser::stmt_expr;
using decl_var = parser::decl_var;
using decl_function = parser::decl_function;
using decl_program = parser::decl_program;

// Returns an array of nodes and the root node index
auto parse(std::string_view code, bool should_be_error = false) {
  lexer lexer(code);
  auto tokens = lexer.lex();
  if (not tokens.has_value())
    UNSCOPED_INFO(to_string(tokens.error()));
  REQUIRE(tokens.has_value());

  parser p(*tokens);
  auto ast = p.parse();
  if (not should_be_error) {
    if (not ast.has_value())
      UNSCOPED_INFO(to_string(ast.error()));
    REQUIRE(ast.has_value());
  } else {
    REQUIRE(not ast.has_value());
  }
  return std::move(ast).value();
}

template<typename T>
bool holds(const node &n) { return std::holds_alternative<T>(n.data); }

template<typename T>
const T &as(const node &n) { return std::get<T>(n.data); }

const node &get_node(const std::vector<node> &nodes, index_t idx) {
  REQUIRE(idx >= 0);
  REQUIRE(idx < static_cast<index_t>(nodes.size()));
  return nodes[idx];
}

std::vector<index_t> collect_list(const std::vector<node> &nodes, index_t head) {
  std::vector<index_t> result;
  auto curr = head;
  while (curr != parser::empty_node) {
    result.push_back(curr);
    curr = nodes[curr].next;
  }
  return result;
}

TEST_CASE("Parser handles empty input", "[parser]") {
  parse("", true);
}

TEST_CASE("Parser rejects garbage input", "[parser]") {
  parse("blah blah", true);
}

TEST_CASE("Parser parses function with empty body", "[parser][function]") {
  auto [nodes, root] = parse("int main() {}");
  REQUIRE(root != parser::empty_node);
  REQUIRE(nodes.size() >= 2);   // функция + блок

  REQUIRE(holds<decl_function>(nodes[root]));
  auto &func = as<decl_function>(nodes[root]);
  CHECK(func.ret_type == "int");
  CHECK(func.name == "main");
  CHECK(func.params_head == parser::empty_node);
  CHECK(func.body != parser::empty_node);

  auto &block = as<stmt_block>(get_node(nodes, func.body));
  CHECK(block.children_head == parser::empty_node);
}

TEST_CASE("Parser parses function with return statement", "[parser][function][statement]") {
  auto [nodes, root] = parse("int main() { return 42; }");
  REQUIRE(root != parser::empty_node);

  auto &func = as<decl_function>(get_node(nodes, root));
  auto &block = as<stmt_block>(get_node(nodes, func.body));
  auto children = collect_list(nodes, block.children_head);
  REQUIRE(children.size() == 1);

  auto &stmt = get_node(nodes, children[0]);
  REQUIRE(holds<stmt_return>(stmt));
  auto &ret = as<stmt_return>(stmt);
  REQUIRE(ret.expr != parser::empty_node);

  auto &expr = get_node(nodes, ret.expr);
  REQUIRE(holds<expr_literal>(expr));
  auto &lit = as<expr_literal>(expr);
  REQUIRE(std::holds_alternative<int64_t>(lit));
  CHECK(std::get<int64_t>(lit) == 42);
}
