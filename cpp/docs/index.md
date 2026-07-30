# Pang-Lang C++ Port Documentation

- [Port Notes](port-notes.md) — Technical notes on the Go/Lua → C++ porting
  process, architectural decisions, and comparison with the Go implementation.

- [Language Reference](../../docs/language-reference.md) — Complete reference of
  all built-in words, syntax, Italian mode, and examples.

- [Source Code](../interpreter.hpp) — Interpreter header with full class
  declaration.
- [Source Code](../interpreter.cpp) — Interpreter implementation.
- [Source Code](../main.cpp) — Entry point.

## Building

```bash
# From the project root:
g++ -std=c++17 -o cpp/pang cpp/main.cpp cpp/interpreter.cpp

# With optimizations:
g++ -std=c++17 -O2 -o cpp/pang cpp/main.cpp cpp/interpreter.cpp
```

## Running

```bash
./cpp/pang                    # REPL mode
./cpp/pang tests/factorial.words  # Run a word-definition file
./cpp/pang italian            # Italian mode REPL
./cpp/pang -                  # Force REPL
./cpp/pang tests/factorial.words -  # Run file then enter REPL
```

[← Back to README](../../README.md)
