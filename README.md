<img width="64" src="./icon.svg" align="left" alt="KorkaVM icon">

# KorkaVM

A Virtual Machine where lexing and compilation happen entirely at **compile-time**.

---

### What is this

KorkaVM is a project where I'm trying to create a tool that allows to embed logic
without runtime overhead of parsing or loading external files. You write C-like code
right inside C++, and the compiler transforms it into internal bytecode before your
program even starts.

### Status

|    Component     |  Stage  | Execution context |
|:----------------:|:-------:|:-----------------:|
|      Lexer       |  Done   |     constexpr     |
| Bytecode builder |  Done   |     constexpr     |
|      Parser      |   WIP   |     constexpr     |
|     Compiler     | Planned |     constexpr     |
|    VM runner     | Planned |      runtime      |


## Example context

```cpp

auto foo(int a) -> int {
  return a * 2 + 5;
}

constexpr auto bindings = korka::make_bindings(
  "foo", &foo
);

constexpr auto my_script = korka::compile(bindings, R"(
    int calculate(int x) {
        if (x <= 0) return 0;

        return foo(x) / 3;
    }
)");

// Simple usage
int main() {
  korka::runtime vm;
  int result = vm.execute(my_script)
}

// Not so simple usage
int main() {
  korka::runtime vm;
  
  // Byte code gets inserted right into the native instruction flow
  // and executed by vm right there
  korka::run_embed<my_script>(vm);
}
```