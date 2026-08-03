# Pang C Port — Overview

## Provenance Chain

```
pangea/src/pangea1/main.lua  (canonical v028)
  └── main.go                  (Go port)
       └── main_go_structured.lua (Lua mirror of Go layout)
            └── cpp/*.{hpp,cpp}   (C++17 port)
                 └── c/*.{h,c}    (C11 port — this directory)
```

## Files

| File       | LOC  | Purpose                                                                                          |
| :--------- | :--- | :----------------------------------------------------------------------------------------------- |
| `pang.h`   | 146  | Header: tagged-union `Value` type, `PangCtx` context struct, API prototypes, dynamic-array types |
| `pang.c`   | 1315 | Full implementation: tokenizer, evaluator, 21 built-in word definitions, REPL, file execution    |
| `main.c`   | 31   | Entry point — parses `italian` flag, calls `pang_init` + `pang_run`                              |
| `Makefile` | 24   | Build system — `gcc -std=c11 -Wall -Wextra -O2`                                                  |

## Build & Run

```bash
cd c
make               # build ./pang
make clean         # remove object files and binary
make test          # run factorial tests
```

```bash
./pang tests/factorial.words              # English mode
./pang italian tests/fattoriale.parole    # Italian mode
echo 'print 42' | ./pang -                # REPL mode piped
./pang -                                  # Interactive REPL
```

## Key Design Decisions

### Tagged Union for Dynamic Values

C++ used `std::variant<monostate, double, bool, string, shared_ptr<Namespace>>`.
The C port uses an explicit tagged-union:

```c
enum ValueType { VAL_NIL, VAL_NUM, VAL_BOOL, VAL_STR, VAL_NS };
typedef struct Value {
    ValueType type;
    union {
        double num;
        bool   boolean;
        char  *str;          // heap-allocated, owned
        struct Namespace *ns; // borrowed pointer into callStack
    };
} Value;
```

### Context Struct Instead of Class

All interpreter state lives in `PangCtx`. Functions take `PangCtx*` as the first
parameter, replacing the C++ `PangInterpreter` class and `this` pointer.

### Dynamic Arrays Replace std::vector / std::map

- `StrVec` / `BoolVec` / `NSVec` — growable typed arrays with capacity doubling
- `wordDefs` — flat array of `{key, def}` pairs, searched linearly (< 30
  entries)
- `Namespace` variables — flat arrays of keys/vals, searched linearly

### setjmp / longjmp for Exit Flow

The C++ version threw `std::runtime_error("exit")` to unwind the call stack. The
C port uses `setjmp`/`longjmp` with a jump buffer stored in
`PangCtx.exit_jmpbuf`.

### Function Pointers Instead of std::function Lambdas

`WordFn` is a plain C function pointer `Value (*fn)(PangCtx*, const int* args)`.
Closures from `define_word` store captured data in `WordDef.closure_body_index`
and set `is_closure = true`.

### Manual Memory Management

The C++ version relied on RAII (`std::shared_ptr`, `std::string`,
`std::vector`). The C port uses explicit `free()`, `pang_strdup()`, and
ownership conventions:

- `VAL_STR` values own their heap string — freed by `value_free()`
- `VAL_NS` values borrow a pointer — never freed by `value_free()`
- Namespace keys/values are owned by the `Namespace` struct — freed by
  `namespace_free()`
- Call-stack frames are pushed/popped, with popped frames freed immediately
