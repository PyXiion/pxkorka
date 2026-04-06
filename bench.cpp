#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <iomanip>
#include <Python.h>
#include <lua.hpp>

#include "korka/compiler/compiler.hpp"
#include "korka/compiler/binding.hpp"
#include "korka/vm/vm_runtime.hpp"

[[gnu::noinline]]
auto native_fib(std::int64_t n) -> std::int64_t {
  if (n == 0) return 0;
  if (n == 1) return 1;

  return native_fib(n - 1) + native_fib(n - 2);
}

const char *py_code = R"(
def fib(n):
    if n == 0: return 0
    if n == 1: return 1
    return fib(n-1) + fib(n-2)
)";

const char *lua_code = R"(
function fib(n)
  if n == 0 then return 0 end
  if n == 1 then return 1 end
  return fib(n-1) + fib(n-2)
end
)";

constexpr char korka_code[] = "int fib(int n) { if (n==0) return 0; if (n==1) return 1; return fib(n-1)+fib(n-2); }";

struct Result {
  int n;
  int iterations;
  double korka_ms;
  double lua_ms;
  double python_ms;
  double cpp_ms;
};

void run_benchmarks() {
  // clang-format off
  // @formatter:off
  const std::vector<std::pair<int, int>> test_cases = {
    {10, 2'000'000},
    {15,   200'000},
    {20,    20'000},
    {23,     4'500},
    {25,     2'000},
    {28,       400},
    {30,       200}
  };
  // @formatter:on
  // clang-format on

  std::vector<Result> results;

  std::cout << "=== Korka vs Lua vs Python vs C++ ===\n\n";

  std::cout << "=== Phase 1: Initialization ===\n";

  auto py_start = std::chrono::high_resolution_clock::now();
  Py_Initialize();
  PyObject *py_main = PyImport_AddModule("__main__");
  PyObject *py_dict = PyModule_GetDict(py_main);
  PyRun_String(py_code, Py_file_input, py_dict, py_dict);
  PyObject *py_fib_func = PyDict_GetItemString(py_dict, "fib");
  auto py_end = std::chrono::high_resolution_clock::now();

  auto lua_start = std::chrono::high_resolution_clock::now();
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  luaL_dostring(L, lua_code);
  auto lua_end = std::chrono::high_resolution_clock::now();

  auto korka_start = std::chrono::high_resolution_clock::now();
  constexpr static auto bindings = korka::make_bindings();
  constexpr auto korka_bytecode = korka::compile<korka_code, &bindings>();
  constexpr auto korka_fib_addr = korka_bytecode.function<"fib">();
  korka::vm::context ctx{korka_bytecode.bytes, bindings};
  auto korka_end = std::chrono::high_resolution_clock::now();

  auto t_py = std::chrono::duration<double, std::micro>(py_end - py_start).count();
  auto t_lua = std::chrono::duration<double, std::micro>(lua_end - lua_start).count();
  auto t_korka = std::chrono::duration<double, std::micro>(korka_end - korka_start).count();

  printf("Python Init: %8.1f µs\n", t_py);
  printf("Lua Init:    %8.1f µs\n", t_lua);
  printf("Korka Init:  %8.1f µs\n\n", t_korka);

  std::cout << "=== Phase 2: Execution (different fib depths) ===\n\n";
  std::cout << std::setw(6) << "n"
            << std::setw(12) << "iters"
            << std::setw(12) << "Korka ms"
            << std::setw(12) << "Lua ms"
            << std::setw(12) << "Python ms"
            << std::setw(12) << "C++ ms"
            << std::setw(14) << "Speedup" << "\n";
  std::cout << std::string(72, '-') << "\n";

  for (const auto &[n, iterations]: test_cases) {
    Result r{.n = n, .iterations = iterations};

    // Korka
    auto k_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      volatile auto res = ctx.call(korka_fib_addr, static_cast<int64_t>(n));
    }
    auto k_end = std::chrono::high_resolution_clock::now();
    r.korka_ms = std::chrono::duration<double, std::milli>(k_end - k_start).count();

    // Lua
    auto l_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      lua_getglobal(L, "fib");
      lua_pushinteger(L, n);
      lua_pcall(L, 1, 1, 0);
      lua_pop(L, 1);
    }
    auto l_end = std::chrono::high_resolution_clock::now();
    r.lua_ms = std::chrono::duration<double, std::milli>(l_end - l_start).count();

    // Python
    auto p_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      PyObject *args = PyTuple_Pack(1, PyLong_FromLong(n));
      PyObject *result = PyObject_CallObject(py_fib_func, args);
      Py_DECREF(args);
      Py_DECREF(result);
    }
    auto p_end = std::chrono::high_resolution_clock::now();
    r.python_ms = std::chrono::duration<double, std::milli>(p_end - p_start).count();

    // C++
    auto cpp_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      volatile auto a = native_fib(n);
      (void) a;
    }
    auto cpp_end = std::chrono::high_resolution_clock::now();
    r.cpp_ms = std::chrono::duration<double, std::milli>(cpp_end - cpp_start).count();

    results.push_back(r);

    double speedup = r.python_ms / r.korka_ms;

    printf("%6d %11d %11.2f %11.2f %11.2f %11.2f %12.2fx\n",
           n, iterations, r.korka_ms, r.lua_ms, r.python_ms, r.cpp_ms, speedup);
  }

  std::ofstream csv("fib_benchmark.csv");
  csv << "n,iterations,korka_ms,lua_ms,python_ms,korka_speedup_vs_python\n";
  for (const auto &r: results) {
    double speedup = r.python_ms / r.korka_ms;
    csv << r.n << "," << r.iterations << ","
        << r.korka_ms << "," << r.lua_ms << "," << r.python_ms << ","
        << speedup << "\n";
  }
  csv.close();

  std::cout << "=== DONE ===";
  Py_Finalize();
  lua_close(L);
}

int main() {
  run_benchmarks();
  return 0;
}