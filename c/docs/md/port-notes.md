# Port Notes — C++17 → C11

This documents the technical decisions made when porting the Pang v028
interpreter from C++17 (`cpp/`) to C11 (`c/`).

## Architecture Mapping

| Aspect               | C++17                                                                  | C11                                                     |
| :------------------- | :--------------------------------------------------------------------- | :------------------------------------------------------ |
| Dynamic values       | `std::variant<monostate, double, bool, string, shared_ptr<Namespace>>` | Tagged union `Value { ValueType type; union { ... }; }` |
| String literals flag | `std::vector<bool>`                                                    | `BoolVec` (custom growable `bool*` array)               |
| Word definitions     | `std::unordered_map<string, WordDef>`                                  | Flat `WordDefsEntry*` array, linear search              |
| Word functions       | `std::function<Value(const vector<int>&)>`                             | Plain `Value (*WordFn)(PangCtx*, const int*)`           |
| Call stack           | `vector<shared_ptr<Namespace>>`                                        | `NSVec` (custom growable `Namespace**` array)           |
| File paths           | `std::filesystem::path`                                                | `strrchr()` + manual string building                    |
| File I/O             | `std::ifstream` + `std::ostringstream`                                 | `fopen` / `fread` / `fclose`                            |
| REPL                 | `std::getline`                                                         | `fgets`                                                 |
| Error handling       | `throw std::runtime_error` → `try/catch`                               | `setjmp` / `longjmp`                                    |
| Class                | `class PangInterpreter`                                                | `PangCtx` struct + `pang_*` function prefix             |
| Memory management    | RAII (destructors, `shared_ptr`)                                       | Explicit `free()`, ownership conventions                |

## Recursive Data Structures

The C++ version used `std::shared_ptr<Namespace>` inside `std::variant` to break
the completeness cycle between `Value` and `Namespace`. The C version takes a
simpler approach: `Value` holds a raw `Namespace*` pointer that is **always
borrowed** from the `callStack` vector.

```
callStack (NSVec)
  └── Namespace*  ───  Namespace { keys[], vals[] }
                          vals[i] → Value { .ns = &another_namespace }
                                          ↑ borrowed, never freed
```

This means `Value` of type `VAL_NS` must never outlive the call-stack frame it
points into. The interpreter ensures this by only pushing/popping at
well-defined boundaries (`define_word` calls, `do...end` block exits).

## Closures via Body Index

C++ lambdas with captures (e.g. `[this, bodyIndex]`) become impossible in plain
C. The solution: `WordDef` has a `bool is_closure` flag and an
`int
closure_body_index` field. When `evaluateWord` encounters a closure word,
it:

1. Evaluates argument indices into a new `Namespace` frame
2. Pushes the frame onto `callStack`
3. Calls `evaluateWord(ctx, def.closure_body_index)` — which sees arguments via
   `argument` built-in
4. Pops and frees the frame

```c
if (def->is_closure) {
    Namespace *frame = namespace_new();
    for (int i = 0; i < def->arity; i++) {
        char idx_str[16];
        snprintf(idx_str, sizeof(idx_str), "%d", i + 1);
        namespace_set(frame, idx_str, evaluate_word(ctx, args[i]));
    }
    nsvec_push(&ctx->call_stack, frame);
    result = evaluate_word(ctx, def->closure_body_index);
    namespace_free(nsvec_pop(&ctx->call_stack));
}
```

## Dynamic Array Implementation

Instead of `std::vector<T>`, the C port uses three typed growable-array structs
with a consistent pattern:

| Struct    | Element type | Push function  | Capacity growth         |
| :-------- | :----------- | :------------- | :---------------------- |
| `StrVec`  | `char*`      | `strvec_push`  | Start 8, double on full |
| `BoolVec` | `bool`       | `boolvec_push` | Start 8, double on full |
| `NSVec`   | `Namespace*` | `nsvec_push`   | Start 8, double on full |

`StrVec` owns its string elements (each `strdup`'d on push, freed on free).
`NSVec` does **not** own — namespaces are pushed/popped manually.

## Translation Table

The C++ `std::unordered_map<string, string>` for Italian translations becomes a
static sorted array of `{en, it}` pairs, searched linearly:

```c
static const Translation translate_table[] = {
    {"print", "stampa"},
    {"exit", "esci"},
    // ... 34 entries total
};
static const int translate_count = 34;
```

With only 34 entries, linear search is fast enough and avoids implementing a
hash table.

## setjmp / longjmp for Exit

The C++ version used `throw std::runtime_error("exit")` to abort execution
cleanly from any depth. The standard C equivalent is `setjmp`/`longjmp`:

```c
// In read_execute_loop / pang_run:
if (setjmp(ctx->exit_jmpbuf) == 0) {
    execute_program(ctx, line_buf);  // normal path
} else {
    // longjmp from word_exit landed here
    if (ctx->exit_requested) break;
}

// In word_exit:
ctx->exit_requested = true;
longjmp(ctx->exit_jmpbuf, 1);
```

Variables modified between `setjmp` and `longjmp` that must retain their value
across the jump are declared `volatile` (`filename`, `repl`, `reopened_tty`).

## Memory Ownership Rules

| Allocation               | Owner                         | Freed by                          |
| :----------------------- | :---------------------------- | :-------------------------------- |
| `VAL_STR` `.str`         | The `Value` struct            | `value_free()`                    |
| Namespace keys           | The `Namespace` struct        | `namespace_free()`                |
| Namespace `VAL_STR` vals | The `Namespace` struct        | `namespace_free()`                |
| Word-def keys            | `word_defs_entries` array     | `pang_destroy()`                  |
| Tokenized words          | `StrVec words`                | `strvec_free()`                   |
| File directory stack     | `StrVec file_directory_stack` | `strvec_free()`                   |
| Call-stack frames        | `NSVec call_stack`            | Popped manually or `pang_destroy` |

## Line Count Comparison

| File           | C++17                 | C11           |
| :------------- | :-------------------- | :------------ |
| Header         | 116 (interpreter.hpp) | 146 (pang.h)  |
| Implementation | 887 (interpreter.cpp) | 1315 (pang.c) |
| Entry point    | 37 (main.cpp)         | 31 (main.c)   |
| **Total**      | **1040**              | **1492**      |

The C port is ~450 lines larger, primarily due to:

- Custom dynamic-array implementations (~100 lines)
- Explicit `free()` / ownership management scattered through word functions
- setjmp/longjmp boilerplate
- No `<algorithm>`, `<filesystem>`, or other standard library conveniences
