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

// --- Native C++ ---
[[gnu::noinline]]
auto native_fib(std::int64_t n) -> std::int64_t {
  if (n == 0) return 0;
  if (n == 1) return 1;
  return native_fib(n - 1) + native_fib(n - 2);
}

[[gnu::noinline]]
auto native_linear(std::int64_t n) -> std::int64_t {
  std::int64_t res = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    res += i;
    asm volatile("" : "+g"(res) : : "memory");
  }
  return res;
}

// --- Sources ---
const char *py_code = R"(
def fib(n):
    if n == 0: return 0
    if n == 1: return 1
    return fib(n-1) + fib(n-2)

def sum_linear(n):
    res = 0
    i = 0
    while i < n:
        res = res + i
        i = i + 1
    return res
)";

const char *lua_code = R"(
function fib(n)
  if n == 0 then return 0 end
  if n == 1 then return 1 end
  return fib(n-1) + fib(n-2)
end

function sum_linear(n)
    local res = 0
    local i = 0
    while i < n do
        res = res + i
        i = i + 1
    end
    return res
end
)";

constexpr char korka_fib_code[] = "int fib(int n) { if (n==0) return 0; if (n==1) return 1; return fib(n-1)+fib(n-2); }";

constexpr char korka_linear_code[] = R"(
int sum_linear(int n) {
    int res = 0;
    int i = 0;
    while (i < n) {
        res = res + i;
        i = i + 1;
    }
    return res;
}
)";

void run_benchmarks() {
  std::cout << "=== Phase 1: Initialization ===\n";

  auto py_init_s = std::chrono::high_resolution_clock::now();
  Py_Initialize();
  PyObject *py_main = PyImport_AddModule("__main__");
  PyObject *py_dict = PyModule_GetDict(py_main);
  PyRun_String(py_code, Py_file_input, py_dict, py_dict);
  PyObject *py_fib_func = PyDict_GetItemString(py_dict, "fib");
  PyObject *py_sum_func = PyDict_GetItemString(py_dict, "sum_linear");
  auto py_init_e = std::chrono::high_resolution_clock::now();

  auto lua_init_s = std::chrono::high_resolution_clock::now();
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  luaL_dostring(L, lua_code);
  auto lua_init_e = std::chrono::high_resolution_clock::now();

  auto korka_init_s = std::chrono::high_resolution_clock::now();
  constexpr static auto bindings = korka::make_bindings();
  constexpr auto bc_fib = korka::compile<korka_fib_code, &bindings>();
  constexpr auto bc_linear = korka::compile<korka_linear_code, &bindings>();
  korka::vm::context ctx_fib{bc_fib.bytes, bindings};
  korka::vm::context ctx_linear{bc_linear.bytes, bindings};
  auto addr_fib = bc_fib.function<"fib">();
  auto addr_linear = bc_linear.function<"sum_linear">();
  auto korka_init_e = std::chrono::high_resolution_clock::now();

  printf("Python Init: %8.1f us\n", std::chrono::duration<double, std::micro>(py_init_e - py_init_s).count());
  printf("Lua Init:    %8.1f us\n", std::chrono::duration<double, std::micro>(lua_init_e - lua_init_s).count());
  printf("Korka Init:  %8.1f us\n\n", std::chrono::duration<double, std::micro>(korka_init_e - korka_init_s).count());

  // --- PHASE 2: FIB ---
  std::cout << "=== Phase 2: Recursive Fibonacci ===\n";
  std::vector<std::pair<int, int>> fib_cases = {/*{10, 1'000'000},
                                                {20, 10'000},
                                                {30, 100}*/};
  printf("%6s %10s %12s %12s %12s %12s %10s\n", "n", "iters", "Korka ms", "Lua ms", "Py ms", "C++ ms", "Speedup");

  for (auto &[n, iters]: fib_cases) {
    auto k_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) volatile auto r = ctx_fib.call(addr_fib, (int64_t) n);
    auto k_e = std::chrono::high_resolution_clock::now();

    auto l_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
      lua_getglobal(L, "fib");
      lua_pushinteger(L, n);
      lua_pcall(L, 1, 1, 0);
      lua_pop(L, 1);
    }
    auto l_e = std::chrono::high_resolution_clock::now();

    auto p_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
      PyObject *args = PyTuple_Pack(1, PyLong_FromLong(n));
      Py_DECREF(PyObject_CallObject(py_fib_func, args));
      Py_DECREF(args);
    }
    auto p_e = std::chrono::high_resolution_clock::now();

    auto c_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) volatile auto r = native_fib(n);
    auto c_e = std::chrono::high_resolution_clock::now();

    double km = std::chrono::duration<double, std::milli>(k_e - k_s).count();
    double lm = std::chrono::duration<double, std::milli>(l_e - l_s).count();
    double pm = std::chrono::duration<double, std::milli>(p_e - p_s).count();
    double cm = std::chrono::duration<double, std::milli>(c_e - c_s).count();
    printf("%6d %10d %12.2f %12.2f %12.2f %12.2f %9.1fx\n", n, iters, km, lm, pm, cm, pm / km);
  }

  // --- PHASE 3: LINEAR SUM ---
  std::cout << "\n=== Phase 3: Linear Sum (While Loop) ===\n";
  std::vector<std::pair<int, int>> linear_cases = {{1000,      300'000},
                                                   {100'000,   3000},
                                                   {1'000'000, 300}};
  printf("%6s %10s %12s %12s %12s %12s %10s\n", "n", "iters", "Korka ms", "Lua ms", "Py ms", "C++ ms", "Speedup");

  for (auto &[n, iters]: linear_cases) {
    auto k_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) volatile auto r = ctx_linear.call(addr_linear, (int64_t) n);
    auto k_e = std::chrono::high_resolution_clock::now();

    auto l_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
      lua_getglobal(L, "sum_linear");
      lua_pushinteger(L, n);
      lua_pcall(L, 1, 1, 0);
      lua_pop(L, 1);
    }
    auto l_e = std::chrono::high_resolution_clock::now();

    auto p_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
      PyObject *args = PyTuple_Pack(1, PyLong_FromLong(n));
      Py_DECREF(PyObject_CallObject(py_sum_func, args));
      Py_DECREF(args);
    }
    auto p_e = std::chrono::high_resolution_clock::now();

    auto c_s = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) volatile auto r = native_linear(n);
    auto c_e = std::chrono::high_resolution_clock::now();

    double km = std::chrono::duration<double, std::milli>(k_e - k_s).count();
    double lm = std::chrono::duration<double, std::milli>(l_e - l_s).count();
    double pm = std::chrono::duration<double, std::milli>(p_e - p_s).count();
    double cm = std::chrono::duration<double, std::milli>(c_e - c_s).count();
    printf("%6d %10d %12.2f %12.2f %12.2f %12.2f %9.1fx\n", n, iters, km, lm, pm, cm, pm / km);
  }

  Py_Finalize();
  lua_close(L);
}

int main() {
  run_benchmarks();
  return 0;
}