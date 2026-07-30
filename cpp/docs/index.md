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

### Piped Input

Commands can be piped to the interpreter. After processing piped input, the REPL
reopens `/dev/tty` to stay open for interactive use. Include `exit` (or `esci`
in Italian mode) to quit after piped commands:

```bash
# Pipe commands then continue interactive REPL:
echo "print 333" | ./cpp/pang

# Pipe commands then exit:
echo "print 333 exit" | ./cpp/pang

# Italian mode with factorial file:
echo "stampa fattoriale 5" | ./cpp/pang italian tests/fattoriale.parole -
echo "stampa fattoriale 5 esci" | ./cpp/pang italian tests/fattoriale.parole -
```

> **⚠️ Do not combine `rlwrap` with piped input.** `rlwrap` expects a real
> terminal; piping stdin breaks it. Use `rlwrap` only for interactive sessions
> (`rlwrap ./cpp/pang`) and pipe directly to `./cpp/pang` for scripted input.

[← Back to README](../../README.md)
