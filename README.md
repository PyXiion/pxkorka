<img width="64" src="media/icon.svg" align="left" alt="Korka icon">

# Korka

A Virtual Machine where lexing and compilation happen entirely at **compile-time**.

---

### What is this

Korka is a project where I'm trying to create a tool that allows to embed logic
without runtime overhead of parsing or loading external files. You write C-like code
right inside C++, and the compiler transforms it into internal bytecode before your
program even starts.

### Status

|    Component     | Stage | Execution context |
|:----------------:|:-----:|:-----------------:|
|      Lexer       | Done  |     constexpr     |
| Bytecode builder | Done  |     constexpr     |
|      Parser      | Done  |     constexpr     |
|     Compiler     | Done  |     constexpr     |
|    VM runner     | Done  |      runtime      |

### Features

+ **Full interop** between Korka & C++, you can bind a C++ function into the script and vice versa.
  Type safety guaranteed.
+ **Fast VM**: in my `fib` benchmark Korka surpasses Lua and Python by 30-40%

```cpp
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


constexpr auto bindings = korka::make_bindings(
  korka::wrap<fib>("cpp_fib"),
  korka::wrap<print_n>("print_n")
);
constexpr auto compile_result = korka::compile<code, &bindings>();

// Extracting function types from code
// It returns a pointer, bc you can't return a type ._.
auto main_func = compile_result.function<"main">();
static_assert(std::is_same_v<decltype(main_func), long (*)()>);

auto foo_func = compile_result.function<"foo">();
static_assert(std::is_same_v<decltype(foo_func), long (*)(long, long)>);
```

### Code example

```cpp
// Functions that are interoped into Korka
auto fib(std::int64_t n) -> std::int64_t {
    if (n == 0) return 0;
    if (n == 1) return 1;
    
    return fib(n - 1) + fib(n - 2);
}

auto print_n(std::int64_t n) -> void {
    std::cout << n << '\n';
}

// C-like code
constexpr char code[] = R"(
int fib(int n) {
  if (n == 0) return 0;
  if (n == 1) return 1;

  return fib(n-1) + cpp_fib(n-2);
}

void print_fib(int n) {
  int result = fib(n);

  print_n(result);
  return;
}
)";

constexpr auto bindings = korka::make_bindings(
    korka::wrap<fib>("cpp_fib"),
    korka::wrap<print_n>("print_n")
);

constexpr auto compile_result = korka::compile<code, &bindings>();

constexpr auto script_fib = compile_result.function<"fib">();
constexpr auto script_print_fib = compile_result.function<"print_fib">();

int main() {
    korka::vm::context ctx{
        compile_result.bytes, bindings
    };
    
    // result will have int64_t type
    auto result = ctx.call(script_fib, 12L);
    std::cout << result << '\n';
    
    ctx.call(script_print_fib, 16L);
}
```

## Benchmark

See the [bench.cpp](https://github.com/PyXiion/pxkorka/blob/master/bench.cpp) for full code.


<details>

<summary>Code</summary>

---

### Python code

```python
def fib(n):
    if n == 0: return 0
    if n == 1: return 1
    return fib(n - 1) + fib(n - 2)
```

### Lua code

```lua
function fib(n)
  if n == 0 then return 0 end
  if n == 1 then return 1 end
  return fib(n-1) + fib(n-2)
end
```

### C code (Korka)

```c
int fib(int n) { 
  if (n==0) return 0; 
  if (n==1) return 1; 
  return fib(n-1) + fib(n-2); 
}
```

---

</details>

### Results:

|  n |  Iterations | Korka (ms) |  Lua (ms) | Python (ms) | Korka Speedup (vs. Python) |
|---:|------------:|-----------:|----------:|------------:|:--------------------------:|
| 10 |   2,000,000 |   6,503.03 |  9,686.56 |    8,988.40 |           1.38x            |
| 15 |     200,000 |   7,113.14 | 10,899.20 |    9,996.17 |           1.41x            |
| 20 |      20,000 |   8,546.09 | 11,765.60 |   10,961.60 |           1.28x            |
| 23 |       4,500 |   7,964.48 | 11,491.50 |   10,667.10 |           1.34x            |
| 25 |       2,000 |   8,900.08 | 12,980.40 |   12,277.30 |           1.38x            |
| 28 |         400 |   7,506.88 | 11,101.50 |   10,820.10 |           1.44x            |
| 30 |         200 |   9,910.64 | 14,334.40 |   13,697.50 |           1.38x            |

Cool graph here:
![Execution time graph](img.png)