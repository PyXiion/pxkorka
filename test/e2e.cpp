#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include "korka/compiler/binding.hpp"
#include "korka/compiler/compiler.hpp"
#include "korka/compiler/lexer.hpp"
#include "korka/compiler/parser.hpp"
#include "korka/vm/vm_runtime.hpp"

using std::int64_t;

namespace {
  // --- C++ side of the tests: bound functions and structs ---

  struct Vec {
    int64_t x;
    int64_t y;
  };

  struct Segment {
    Vec a;
    Vec b;
  };

  struct Unused {
    int64_t value;
  };

  auto bound_add(int64_t a, int64_t b) -> int64_t { return a + b; }

  auto vec_add(Vec l, Vec r) -> Vec { return {l.x + r.x, l.y + r.y}; }

  auto make_segment(Vec a, Vec b) -> Segment { return {a, b}; }

  auto segment_length_sq(Segment s) -> int64_t {
    const auto dx = s.b.x - s.a.x;
    const auto dy = s.b.y - s.a.y;
    return dx * dx + dy * dy;
  }
}

template<>
struct korka::struct_def<Vec> {
  static constexpr std::string_view name = "Vec";
  static constexpr auto fields = std::tuple{
    korka::field{"x", &Vec::x},
    korka::field{"y", &Vec::y},
  };
};

template<>
struct korka::struct_def<Segment> {
  static constexpr std::string_view name = "Segment";
  static constexpr auto fields = std::tuple{
    korka::field{"a", &Segment::a},
    korka::field{"b", &Segment::b},
  };
};

template<>
struct korka::struct_def<Unused> {
  static constexpr std::string_view name = "Unused";
  static constexpr auto fields = std::tuple{
    korka::field{"value", &Unused::value},
  };
};

namespace {
  constexpr auto test_bindings = korka::make_bindings(
    korka::wrap<bound_add>("bound_add"),
    korka::wrap<vec_add>("vec_add"),
    korka::wrap<make_segment>("make_segment"),
    korka::wrap<segment_length_sq>("segment_length_sq"),
    korka::expose<Unused>()
  );

  // --- Runtime pipeline: source -> lexer -> parser -> compiler -> VM ---

  constexpr korka::bindings<0, 0> no_bindings{};

  // The bytecode and a context that runs it
  template<class Bindings>
  struct script {
    std::vector<std::byte> bytes;
    korka::flat_map<std::string_view, korka::function_info> functions;
    korka::vm::context<Bindings> ctx;

    script(korka::compilation_result result, const Bindings &bindings, std::size_t stack_slots)
      : bytes(std::move(result.bytes)), functions(std::move(result.functions)),
        ctx(bytes, bindings, stack_slots) {
    }

    template<class Signature, class ...Args>
    auto call(std::string_view name, Args ...args) {
      const auto function = functions.find(name);
      REQUIRE(function != functions.end());
      const korka::vm::function_handle<Signature> handle{function->second.start_pos};

      if constexpr (std::is_void_v<typename korka::function_traits<Signature>::return_type>) {
        ctx.call(handle, args...);
        CHECK(ctx.stack_depth() == 0);
      } else {
        auto result = ctx.call(handle, args...);
        CHECK(ctx.stack_depth() == 0);
        return result;
      }
    }
  };

  constexpr auto default_stack = korka::vm::context_base::default_stack_slots;

  template<class Bindings = std::remove_cvref_t<decltype(no_bindings)>>
  auto build(std::string_view source, const Bindings &bindings = no_bindings, std::size_t stack_slots = default_stack)
    -> std::expected<script<Bindings>, korka::error_t> {
    auto tokens = korka::lexer{source}.lex();
    if (not tokens) return std::unexpected{tokens.error()};

    auto parsed = korka::parser{std::span<const korka::lex_token>{*tokens}}.parse();
    if (not parsed) return std::unexpected{parsed.error()};

    auto compiled = korka::compiler{
      std::span<const korka::nodes::node>{parsed->first}, parsed->second, bindings
    }.compile();
    if (not compiled) return std::unexpected{compiled.error()};

    return std::expected<script<Bindings>, korka::error_t>{
      std::in_place, std::move(*compiled), bindings, stack_slots
    };
  }

  template<class Bindings = std::remove_cvref_t<decltype(no_bindings)>>
  auto build_ok(std::string_view source, const Bindings &bindings = no_bindings, std::size_t stack_slots = default_stack)
    -> script<Bindings> {
    auto result = build(source, bindings, stack_slots);
    INFO((result ? std::string{} : korka::to_string(result.error())));
    REQUIRE(result.has_value());
    return std::move(*result);
  }

  template<class Error>
  constexpr auto error_index = korka::error_t{Error{}}.index();

  using int_fn = int64_t();
  using int_int_fn = int64_t(int64_t);
  using int_int_int_fn = int64_t(int64_t, int64_t);
}

TEST_CASE("T1: control flow and operators", "[e2e]") {
  auto s = build_ok(R"(
    int fib(int n) {
      if (n < 2) return n;
      return fib(n - 1) + fib(n - 2);
    }

    int sum(int n) {
      int total = 0;
      int i = 1;
      while (i <= n) {
        total = total + i;
        i = i + 1;
      }
      return total;
    }

    int arithmetic(int a, int b) { return (a + 3 * b - 1) % 5 + a / b; }
    int neg(int a) { return -a; }
    int plus(int a) { return +a; }
    int not_(int a) { return !a; }
    int ne(int a, int b) { return a != b; }
    int eq(int a, int b) { return a == b; }
    int lt(int a, int b) { return a < b; }
    int le(int a, int b) { return a <= b; }
    int gt(int a, int b) { return a > b; }
    int ge(int a, int b) { return a >= b; }
    int mod(int a, int b) { return a % b; }
    int both(int a, int b) { return a and b; }
    int either(int a, int b) { return a or b; }
    int precedence() { return 1 or 0 and 0; }

    // The right side must not run when the left one decides
    int skipped_and() { int a = 0; int r = 0 and (a = 1); return a * 10 + r; }
    int skipped_or() { int a = 0; int r = 1 or (a = 1); return a * 10 + r; }
    int taken_and() { int a = 0; int r = 1 and (a = 5); return a * 10 + r; }
    int taken_or() { int a = 0; int r = 0 or (a = 3); return a * 10 + r; }

    int uses_later() { return later() + 1; }
    int later() { return 41; }
    int is_even(int n) { if (n == 0) return 1; return is_odd(n - 1); }
    int is_odd(int n) { if (n == 0) return 0; return is_even(n - 1); }

    int shadow() { int a = 1; { int a = 2; a = a + 10; } return a; }
    int shadow_init() { int a = 1; int r = 0; { int a = a + 1; r = a; } return r * 10 + a; }

    int many_locals() {
      int a = 1; int b = 2; int c = 3; int d = 4; int e = 5;
      int f = 6; int g = 7; int h = 8; int i = 9; int j = 10;
      return a + b + c + d + e + f + g + h + i + j;
    }
  )");

  SECTION("recursion and loops") {
    CHECK(s.call<int_int_fn>("fib", 0) == 0);
    CHECK(s.call<int_int_fn>("fib", 15) == 610);
    CHECK(s.call<int_int_fn>("sum", 100) == 5050);
    CHECK(s.call<int_int_fn>("sum", 0) == 0);
  }

  SECTION("operators") {
    CHECK(s.call<int_int_int_fn>("arithmetic", 7, 2) == (7 + 3 * 2 - 1) % 5 + 7 / 2);
    CHECK(s.call<int_int_fn>("neg", 5) == -5);
    CHECK(s.call<int_int_fn>("plus", 5) == 5);
    CHECK(s.call<int_int_fn>("not_", 0) == 1);
    CHECK(s.call<int_int_fn>("not_", 9) == 0);
    CHECK(s.call<int_int_int_fn>("ne", 1, 2) == 1);
    CHECK(s.call<int_int_int_fn>("ne", 2, 2) == 0);
    CHECK(s.call<int_int_int_fn>("eq", 2, 2) == 1);
    CHECK(s.call<int_int_int_fn>("lt", 1, 2) == 1);
    CHECK(s.call<int_int_int_fn>("lt", 2, 2) == 0);
    CHECK(s.call<int_int_int_fn>("le", 2, 2) == 1);
    CHECK(s.call<int_int_int_fn>("le", 3, 2) == 0);
    CHECK(s.call<int_int_int_fn>("gt", 3, 2) == 1);
    CHECK(s.call<int_int_int_fn>("gt", 2, 2) == 0);
    CHECK(s.call<int_int_int_fn>("ge", 2, 2) == 1);
    CHECK(s.call<int_int_int_fn>("ge", 1, 2) == 0);
    CHECK(s.call<int_int_int_fn>("mod", 17, 5) == 2);
    CHECK(s.call<int_int_int_fn>("mod", -7, 3) == -1);
    CHECK(s.call<int_int_int_fn>("both", 5, 7) == 1);
    CHECK(s.call<int_int_int_fn>("both", 5, 0) == 0);
    CHECK(s.call<int_int_int_fn>("either", 0, 0) == 0);
    CHECK(s.call<int_int_int_fn>("either", 2, 0) == 1);
    CHECK(s.call<int_int_int_fn>("either", 0, 9) == 1);
    CHECK(s.call<int_fn>("precedence") == 1);
  }

  SECTION("short circuit") {
    CHECK(s.call<int_fn>("skipped_and") == 0);
    CHECK(s.call<int_fn>("skipped_or") == 1);
    CHECK(s.call<int_fn>("taken_and") == 51);
    CHECK(s.call<int_fn>("taken_or") == 31);
  }

  SECTION("functions defined later") {
    CHECK(s.call<int_fn>("uses_later") == 42);
    CHECK(s.call<int_int_fn>("is_even", 10) == 1);
    CHECK(s.call<int_int_fn>("is_even", 7) == 0);
    CHECK(s.call<int_int_fn>("is_odd", 7) == 1);
  }

  SECTION("scopes and locals") {
    CHECK(s.call<int_fn>("shadow") == 1);
    CHECK(s.call<int_fn>("shadow_init") == 21);
    CHECK(s.call<int_fn>("many_locals") == 55);
  }
}

TEST_CASE("T2: value discipline", "[e2e]") {
  // A small stack: a statement that leaks a slot would overflow it in the loop below
  auto s = build_ok(R"(
    int one() { return 1; }

    int chain() { int a; int b; a = b = 5; return a * 10 + b; }
    int assign_condition() { int a = 0; if (a = 5) return a; return 0 - 1; }
    int assign_zero_condition() { int a = 3; if (a = 0) return 1; return a + 7; }

    int discards() {
      int i = 0;
      while (i < 5000) {
        one();
        1 + 2;
        i = i + 1;
        i = i + 0;
      }
      return i;
    }

    void no_return() { int a = 1; a = a + 1; }
    int after_void() { no_return(); return 7; }
    int unrelated() { return 99; }

    int empty_statements() { ; ; int a = 3; ; return a; }
    int empty_body() { int i = 0; while (i < 3) i = i + 1; if (i) ; else ; return i; }

    int unbounded(int n) { return unbounded(n + 1) + 1; }
  )", no_bindings, 2048);

  SECTION("assignment is an expression") {
    CHECK(s.call<int_fn>("chain") == 55);
    CHECK(s.call<int_fn>("assign_condition") == 5);
    CHECK(s.call<int_fn>("assign_zero_condition") == 7);
  }

  SECTION("expression statements drop their value") {
    CHECK(s.call<int_fn>("discards") == 5000);
  }

  SECTION("void functions return on their own") {
    s.call<void()>("no_return");
    CHECK(s.call<int_fn>("after_void") == 7);
    CHECK(s.call<int_fn>("unrelated") == 99);
  }

  SECTION("empty statements") {
    CHECK(s.call<int_fn>("empty_statements") == 3);
    CHECK(s.call<int_fn>("empty_body") == 3);
  }

  SECTION("a stack overflow is reported and the context stays usable") {
    CHECK_THROWS_AS(s.call<int_int_fn>("unbounded", 0), korka::vm::stack_overflow);
    CHECK(s.ctx.stack_depth() == 0);
    CHECK(s.call<int_fn>("unrelated") == 99);
  }
}

TEST_CASE("T3: rejected programs", "[e2e]") {
  using namespace korka::error;

  struct row {
    std::string_view name;
    std::string_view source;
    std::size_t error;
  };

  const row rows[] = {
    {"D4: bare return in an int function", "int f() { return; }", error_index<type_mismatch>},
    {"D4: no return at all", "int f() { int a = 1; }", error_index<missing_return>},
    {"D4: return on one branch only", "int f(int a) { if (a) return 1; }", error_index<missing_return>},
    {"D4: return in a loop only", "int f(int a) { while (a) { return 1; } }", error_index<missing_return>},
    {"D5: void initialiser", "void v() {} int f() { int a = v(); return a; }", error_index<type_mismatch>},
    {"D6: too few arguments", "int f(int a) { return a; } int g() { return f(); }", error_index<argument_count_mismatch>},
    {"D6: too many arguments", "int f(int a) { return a; } int g() { return f(1, 2); }",
     error_index<argument_count_mismatch>},
    {"D6: bound function arity", "int g() { return bound_add(1); }", error_index<argument_count_mismatch>},
    {"D6: void argument", "void v() {} int f(int a) { return a; } int g() { return f(v()); }",
     error_index<type_mismatch>},
    {"D8: two definitions", "int f() { return 1; } int f() { return 2; }", error_index<redeclaration>},
    {"D8: clash with a bound function", "int bound_add(int a, int b) { return a + b; }", error_index<redeclaration>},
    {"D8: same scope twice", "int f() { int a; int a; return a; }", error_index<redeclaration>},
    {"D8: parameter redeclared", "int f(int a) { int a = 1; return a; }", error_index<redeclaration>},
    {"D9: unknown type", "int f() { foo x = 3; return 0; }", error_index<unknown_type>},
    {"D12: missing initialiser", "int f() { int a = ; return a; }", error_index<other_parser_error>},
    {"undefined variable", "int f() { return a; }", error_index<undefined_symbol>},
    {"undefined function", "int f() { return g(); }", error_index<undefined_symbol>},
    {"assignment to a call", "struct P { int x; }; P p() { P v; return v; } int f() { p().x = 1; return 0; }",
     error_index<not_assignable>},
    {"unknown field", "struct P { int x; }; int f() { P p; return p.y; }", error_index<unknown_field>},
    {"member of an int", "int f() { int a; return a.x; }", error_index<member_on_non_struct>},
    {"struct type mismatch",
     "struct A { int x; }; struct B { int x; }; int f() { A a; B b; a = b; return 0; }",
     error_index<type_mismatch>},
    {"struct argument mismatch",
     "struct A { int x; }; struct B { int x; }; int g(A a) { return a.x; } int f() { B b; return g(b); }",
     error_index<type_mismatch>},
    {"struct in a condition", "struct A { int x; }; int f() { A a; if (a) return 1; return 0; }",
     error_index<type_mismatch>},
    {"struct in arithmetic", "struct A { int x; }; int f() { A a; return a + 1; }", error_index<type_mismatch>},
    {"struct as an int", "struct A { int x; }; int f() { A a; return a; }", error_index<type_mismatch>},
    {"redeclared struct", "struct A { int x; }; struct A { int y; };", error_index<redeclaration>},
    {"script struct named like a bound one", "struct Vec { int z; };", error_index<redeclaration>},
    {"recursive struct", "struct A { A next; };", error_index<unknown_type>},
    {"unknown field type", "struct A { B b; };", error_index<unknown_type>},
    {"empty struct", "struct A { };", error_index<other_parser_error>},
    {"void parameter", "int f(void a) { return 0; }", error_index<other_compiler_error>},
  };

  for (const auto &r: rows) {
    INFO(r.name);
    auto result = build(r.source, test_bindings);
    REQUIRE_FALSE(result.has_value());
    INFO(korka::to_string(result.error()));
    CHECK(result.error().index() == r.error);
  }
}

TEST_CASE("T4: script structs", "[e2e]") {
  auto s = build_ok(R"(
    struct Vec { int x; int y; };
    struct Line { Vec a; Vec b; };

    Vec add(Vec l, Vec r) {
      Vec out;
      out.x = l.x + r.x;
      out.y = l.y + r.y;
      return out;
    }

    int main() {
      Line ln;
      ln.a.x = 1; ln.a.y = 2;
      ln.b = add(ln.a, ln.a);
      Vec copy = ln.b;
      return copy.x + add(copy, copy).y;   // member of a temporary
    }

    Vec make(int x, int y) { Vec v; v.x = x; v.y = y; return v; }
    Line make_line() { Line l; l.a = make(1, 2); l.b = make(3, 4); return l; }

    int copies() { Vec a; a.x = 1; Vec b = a; b.x = 9; return a.x * 10 + b.x; }
    int by_value() { Vec v = make(1, 2); return modify(v) * 1000 + v.x; }
    int modify(Vec v) { v.x = v.x + 100; return v.x + v.y; }
    int temporary() { return make(3, 4).y; }
    int nested_temporary() { return make_line().b.y * 10 + make_line().a.x; }
    int chained_assign() { Vec a; Vec b; a = b = make(5, 6); return a.x + b.y; }
    int assign_value() { Vec a; return (a = make(7, 8)).y; }
    int fresh_in_loop() {
      int i = 0; int seen = 0;
      while (i < 3) { Vec v; seen = seen + v.x + v.y; v.x = 5; v.y = 5; i = i + 1; }
      return seen;
    }
    int later_struct() { Later l; l.v = 4; return l.v; }
    struct Later { int v; };
  )");

  CHECK(s.call<int_fn>("main") == 10);
  CHECK(s.call<int_fn>("copies") == 19);
  CHECK(s.call<int_fn>("by_value") == 103001);
  CHECK(s.call<int_fn>("temporary") == 4);
  CHECK(s.call<int_fn>("nested_temporary") == 41);
  CHECK(s.call<int_fn>("chained_assign") == 11);
  CHECK(s.call<int_fn>("assign_value") == 8);
  CHECK(s.call<int_fn>("fresh_in_loop") == 0);
  CHECK(s.call<int_fn>("later_struct") == 4);
}

namespace {
  constexpr char bound_code[] = R"(
    struct Box { Vec min; Vec max; };

    Vec scaled(Vec v, int k) { Vec r; r.x = v.x * k; r.y = v.y * k; return r; }
    Vec sum3(Vec a, Vec b, Vec c) { return vec_add(vec_add(a, b), c); }
    Vec shifted(Vec v) { return vec_add(v, scaled(v, 2)); }
    int seg_len_sq(Segment s) { return segment_length_sq(s); }
    int segment_from_script() {
      Vec a; a.x = 1; a.y = 1;
      Vec b; b.x = 4; b.y = 5;
      Segment s = make_segment(a, b);
      return segment_length_sq(s) * 100 + s.b.y;
    }
    int unused_works() { Unused u; u.value = 3; return u.value + bound_add(1, 2); }

    // Its signature has a struct that only exists in the script: it cannot be exported to C++,
    // which must not stop the other functions from being exported
    int width(Box b) { return b.max.x - b.min.x; }
    int box_width() { Box b; b.min.x = 2; b.max = scaled(b.min, 5); return width(b); }
  )";

  constexpr auto bound_result = korka::compile<bound_code, &test_bindings>();

  constexpr auto scaled_fn = bound_result.function<"scaled">();
  constexpr auto sum3_fn = bound_result.function<"sum3">();
  constexpr auto shifted_fn = bound_result.function<"shifted">();
  constexpr auto seg_len_fn = bound_result.function<"seg_len_sq">();
  constexpr auto segment_fn = bound_result.function<"segment_from_script">();
  constexpr auto unused_fn = bound_result.function<"unused_works">();
  constexpr auto box_width_fn = bound_result.function<"box_width">();

  static_assert(std::is_same_v<decltype(scaled_fn)::signature_t, Vec(Vec, int64_t)>);
  static_assert(std::is_same_v<decltype(sum3_fn)::signature_t, Vec(Vec, Vec, Vec)>);
  static_assert(std::is_same_v<decltype(seg_len_fn)::signature_t, int64_t(Segment)>);
  static_assert(std::is_same_v<decltype(box_width_fn)::signature_t, int64_t()>);
}

TEST_CASE("T5: bound structs", "[e2e]") {
  korka::vm::context ctx{bound_result.bytes, test_bindings};

  SECTION("struct arguments and results of script functions") {
    const auto scaled = ctx.call(scaled_fn, Vec{3, 4}, 10);
    CHECK(scaled.x == 30);
    CHECK(scaled.y == 40);

    const auto sum = ctx.call(sum3_fn, Vec{1, 2}, Vec{10, 20}, Vec{100, 200});
    CHECK(sum.x == 111);
    CHECK(sum.y == 222);

    const auto shifted = ctx.call(shifted_fn, Vec{1, 2});
    CHECK(shifted.x == 3);
    CHECK(shifted.y == 6);
  }

  SECTION("nested bound structs") {
    CHECK(ctx.call(seg_len_fn, Segment{{1, 1}, {4, 5}}) == 25);
    CHECK(ctx.call(segment_fn) == 25 * 100 + 5);
  }

  SECTION("exposed without being in a signature, bound function with plain ints") {
    CHECK(ctx.call(unused_fn) == 6);
  }

  SECTION("a script struct with a bound field") {
    CHECK(ctx.call(box_width_fn) == 8);
  }

  CHECK(ctx.stack_depth() == 0);
}

#ifdef __cpp_impl_reflection
namespace {
  // No korka::struct_def specialisation: the fields come from reflection
  struct RVec {
    int64_t x;
    int64_t y;
  };

  struct RSegment {
    RVec a;
    RVec b;
  };

  auto rvec_add(RVec l, RVec r) -> RVec { return {l.x + r.x, l.y + r.y}; }

  auto make_rsegment(RVec a, RVec b) -> RSegment { return {a, b}; }

  constexpr auto reflected_bindings = korka::make_bindings(
    korka::wrap<rvec_add>("rvec_add"),
    korka::wrap<make_rsegment>("make_rsegment")
  );

  constexpr char reflected_code[] = R"(
    struct Pair { RVec first; RVec second; };

    RVec scaled(RVec v, int k) { RVec r; r.x = v.x * k; r.y = v.y * k; return r; }
    RVec sum(RVec a, RVec b) { return rvec_add(a, b); }
    int diagonal() {
      RVec a; a.x = 1; a.y = 2;
      RVec b; b.x = 4; b.y = 6;
      RSegment s = make_rsegment(a, b);
      return s.b.x - s.a.x + (s.b.y - s.a.y) * 10;
    }
    int in_pair() { Pair p; p.first.x = 3; p.second = scaled(p.first, 4); return p.second.x; }
  )";

  constexpr auto reflected_result = korka::compile<reflected_code, &reflected_bindings>();

  constexpr auto r_scaled_fn = reflected_result.function<"scaled">();
  constexpr auto r_sum_fn = reflected_result.function<"sum">();
  constexpr auto r_diagonal_fn = reflected_result.function<"diagonal">();
  constexpr auto r_in_pair_fn = reflected_result.function<"in_pair">();

  static_assert(std::is_same_v<decltype(r_scaled_fn)::signature_t, RVec(RVec, int64_t)>);
}

TEST_CASE("T6: reflection", "[e2e]") {
  static_assert(korka::struct_def<RVec>::name == "RVec");

  korka::vm::context ctx{reflected_result.bytes, reflected_bindings};

  const auto scaled = ctx.call(r_scaled_fn, RVec{3, 4}, 10);
  CHECK(scaled.x == 30);
  CHECK(scaled.y == 40);

  const auto sum = ctx.call(r_sum_fn, RVec{1, 2}, RVec{10, 20});
  CHECK(sum.x == 11);
  CHECK(sum.y == 22);

  CHECK(ctx.call(r_diagonal_fn) == 3 + 4 * 10);
  CHECK(ctx.call(r_in_pair_fn) == 12);
  CHECK(ctx.stack_depth() == 0);
}
#endif
