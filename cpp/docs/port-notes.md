# Port Notes — Go/Lua → C++17

This documents the technical decisions made when porting the Pang v028
interpreter from Go (and its Lua mirror `main_go_structured.lua`) to C++17.

## Architecture

| Aspect               | Go                          | C++17                                                                                          |
| -------------------- | --------------------------- | ---------------------------------------------------------------------------------------------- |
| Dynamic values       | `interface{}`               | `std::variant<std::monostate, double, bool, std::string, Namespace*>`                          |
| String literals flag | `[]bool`                    | `std::vector<bool>`                                                                            |
| Word definitions     | Global `map[string]WordDef` | `std::unordered_map<std::string, WordDef>` (class member)                                      |
| Call stack           | `[]map[string]interface{}`  | `std::vector<Namespace>` where `Namespace` is a struct wrapping `unordered_map<string, Value>` |
| File paths           | Manual string ops           | `std::filesystem::path` (C++17)                                                                |
| File I/O             | `os.ReadFile`               | `std::ifstream` + `std::ostringstream`                                                         |
| REPL                 | `bufio.Scanner`             | `std::getline`                                                                                 |
| Function objects     | Closures (Go)               | `std::function<Value(const std::vector<int>&)>` lambdas                                        |

## Recursive Variant Design

The `Value` type is a recursive variant — a value can be a `Namespace*` (pointer
to a `Namespace`), and a `Namespace` contains `map<string, Value>`. C++ requires
special handling for recursive `std::variant`:

```cpp
struct Namespace;                          // forward declaration
using Value = std::variant<
    std::monostate, double, bool,
    std::string, Namespace*                // pointer breaks circular completeness
>;
struct Namespace {
    std::unordered_map<std::string, Value> vars;
};
```

Using a pointer (`Namespace*`) breaks the circular completeness requirement of
`std::variant` and `std::unordered_map`. The `callStack` vector owns all
`Namespace` objects; `Value` merely points into them. This mirrors the Lua
version's table references.

## Rvalue Reference Pitfall

A common C++ gotcha when using `std::get<T>(expr)` where `expr` is a temporary
(rvalue): the non-const lvalue reference overload is selected, which fails. The
fix is to capture the result in a local variable first:

```cpp
// ❌ ERROR: binding reference to temporary
Namespace *ns = std::get<Namespace*>(evaluateWord(args[0]));

// ✅ Store in local first
Value nsVal = evaluateWord(args[0]);
Namespace *ns = std::get<Namespace*>(nsVal);
```

## Modern C++ Features Used

| Feature             | Where                | Why                                                                    |
| ------------------- | -------------------- | ---------------------------------------------------------------------- |
| `std::variant`      | `Value` type         | Type-safe union replacing `interface{}`                                |
| `std::visit`        | `print` word         | Pattern-matching over variant alternatives                             |
| Structured bindings | `?` word             | `for (const auto &[word, wd] : wordDefs)`                              |
| Lambda captures     | All word functions   | `[this]` for interpreter access, `[this, bodyIndex]` for `define_word` |
| `std::string_view`  | Tokenizer input      | Zero-copy parameter passing                                            |
| `std::filesystem`   | Path utilities       | Clean, portable path operations                                        |
| `std::optional`     | File reading         | Explicit nullable return (vs Go's `err != nil`)                        |
| `constexpr if`      | `std::visit` visitor | Compile-time branching on variant alternatives                         |
| Range-for + auto    | Various loops        | Cleaner iteration                                                      |

## Differences from Go Version

1. **Class-based**: All state is encapsulated in `PangInterpreter` rather than
   file-level globals, making the code more testable and allowing multiple
   interpreter instances.

2. **`Namespace` struct**: Go's `map[string]interface{}` is represented as a
   `struct Namespace { vars: map<string, Value> }` for clarity.

3. **Print formatting**: The `print` word uses `std::visit` with `constexpr if`
   to dispatch on variant type, giving compile-time safety for print formatting.

4. **No helper `#include <cstring>` creep**: Avoided `sprintf`, `strerror` etc.
   by using only `strtod` from `<cstdlib>` and output streams.

## Line Count Comparison

| File                | Go            | C++                   |
| ------------------- | ------------- | --------------------- |
| Header/declarations | N/A           | 110 (interpreter.hpp) |
| Implementation      | 626 (main.go) | 518 (interpreter.cpp) |
| Entry point         | (same file)   | 29 (main.cpp)         |
| **Total**           | **626**       | **657**               |

The C++ port is approximately the same size but split across three files for
better modularity.
