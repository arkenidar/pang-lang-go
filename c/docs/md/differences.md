# Differences — C Port vs C++ Port

This documents behavioral and structural differences between the C++17 port
(`cpp/`) and the C11 port (`c/`).

## Structural Differences

| Aspect         | C++17                                 | C11                                             |
| :------------- | :------------------------------------ | :---------------------------------------------- |
| Organization   | Class `PangInterpreter` with methods  | `PangCtx` struct + `pang_*` free functions      |
| Header guard   | `#ifndef PANG_INTERPRETER_HPP`        | `#ifndef PANG_H`                                |
| Namespace      | `namespace pang { ... }`              | None (C has no namespaces)                      |
| Build          | `g++ -std=c++17`                      | `gcc -std=c11`                                  |
| Math library   | `<cmath>` — `std::fmod` etc. not used | `<math.h>` — linked with `-lm`                  |
| String copy    | Implicit via `std::string`            | Explicit `pang_strdup()` / `strdup()`           |
| Initialization | Constructor + member initializers     | `memset(&ctx, 0, sizeof(ctx))` in `pang_init()` |

## Behavioral Differences

### Exit Handling

The C++ version throws `std::runtime_error("exit")` which unwinds the stack via
C++ exception handling. The C version uses `setjmp`/`longjmp` with `volatile`
variables to achieve the same effect. The `exit_requested` flag is checked after
the `longjmp` to distinguish a clean exit from other error conditions.

### Variant Access

The C++ version uses `std::get_if<T>(&v)` for type-safe variant access. The C
version uses an explicit `switch (val.type)` with manual field access:

```c
// C++:
if (auto *s = std::get_if<std::string>(&val)) { ... }

// C:
if (val.type == VAL_STR) { ... val.str ... }
```

If a C word function receives a Value of unexpected type, it silently returns
`VAL_NIL` or `0.0` rather than throwing. This matches the Go version's behavior
with `.(float64)` type assertions.

### String Literal Parsing

Both ports handle the same escape sequences (`\"`, `\\`, `\n`, `\t`). The C
version uses a growable char buffer with manual realloc rather than
`std::string::operator+=`.

### REPL Reopen

Both ports attempt to reopen `/dev/tty` on EOF once. The C version uses
`freopen("/dev/tty", "r", stdin)` identically to the C++ version.

### Output Formatting

The C++ `print` word uses `std::visit` with `constexpr if` to dispatch on value
type. The C version uses an explicit `switch (val.type)` with `printf` format
strings. Output is identical:

```c
case VAL_NIL:  printf("nil\n"); break;
case VAL_NUM:  printf("%g\n", val.num); break;       // matches C++ std::cout << v
case VAL_BOOL: printf("%s\n", val.boolean ? "true" : "false"); break;
case VAL_STR:  printf("%s\n", val.str); break;
case VAL_NS:   printf("[namespace]\n"); break;
```

### Number Parsing

Both ports use `strtod()` for number detection. The C port omits the C++
`std::strtod` namespace prefix since C's `strtod` from `<stdlib.h>` is the same
function.

## Missing C++ Features and Adaptations

| C++ Feature                     | C Adaptation                                                   |
| :------------------------------ | :------------------------------------------------------------- |
| `std::optional<string>`         | `char*` returning `NULL` on failure                            |
| Structured bindings             | Manual `[i]` index access                                      |
| Range-for (`for (auto& x : v)`) | `for (int i = 0; i < len; i++)`                                |
| `std::string_view` (zero-copy)  | `const char*` (no copying happens anyway with string literals) |
| Lambda captures                 | `WordDef.closure_body_index` + `is_closure` flag               |
| `std::ostringstream`            | `snprintf()` into malloc'd buffer                              |
| `std::filesystem::path`         | `strrchr()` + manual path building                             |
| `auto` type deduction           | Explicit type declarations                                     |
| `constexpr if`                  | Regular `if` / `switch`                                        |
| Destructors / RAII              | Explicit `free()` calls, ownership conventions                 |
| `std::move`                     | Struct assignment (shallow copy)                               |

## Compatibility

The C port passes exactly the same command-line arguments as the C++ port:

```
./pang [italian] [filename] [--repl|-]
```

All `.words` and `.parole` test files produce identical output across all three
ports (Go, C++17, C11).
