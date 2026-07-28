# Ark: Evolutionary Snapshots

The `ark/` directory contains Go ports of key historical versions of the pang
interpreter, mirroring the Lua archive at `pangea/ark/lua/`.

Each snapshot is a standalone, runnable Go program that captures the language at
a specific point in its evolution.

## Snapshot Index

| File              | LOC | Key Feature Added                                                                      |
| :---------------- | :-- | :------------------------------------------------------------------------------------- |
| `ark/pang-000.go` | 48  | `phrase_length` — computes expression span via word arities                            |
| `ark/pang-001.go` | 58  | `do`/`end` block support for sequential evaluation                                     |
| `ark/pang-002.go` | 105 | `evaluate_word` — first working interpreter (`print`, `add`, `do`)                     |
| `ark/pang-003.go` | 124 | `true`/`false`/`if` — conditional branching                                            |
| `ark/pang-004.go` | 156 | `while`, `not`, `equal`, `set`/`get`, `string`, `modulus`, `<=` — **FizzBuzz-capable** |
| `ark/pang-006.go` | 200 | `greater`, `execute_program()`, file loading, REPL loop                                |
| `ark/pang-010.go` | 243 | `multiply`, `define_word`/`argument`, `call_stack` scoping, `dont` — recursion works   |

## Running a Snapshot

```bash
go run ark/pang-000.go   # phrase_length: prints 4
go run ark/pang-004.go   # full FizzBuzz output
go run ark/pang-006.go   # file execution or REPL
```

## Canonical Version

The canonical v028 interpreter is `main.go` (615 LOC), which adds:

- String literals with escape sequences (`"hello\nworld"`)
- `repeat`, `read_text`, `to_number`, `increment`
- `variable_set`/`variable_get` (namespaced variables)
- `command_prompt`, `!`/`?` operators
- File include resolution with `file_directory_stack`
- Hashbang (`#!`) support
- Full Italian translation layer
- Clean `string_literals` tracking instead of special-cased phrase length

## Origin

These files are ports of the corresponding Lua versions in
`pangea/ark/lua/pang-*.lua`, which preserve the original development history.

Note: Versions 005, 007-009 were skipped in the Go archive as they represent
transitional/broken intermediate states (e.g., `define_word` without working
`call_stack`, debug-only releases). The canonical `main.go` represents the
stable v028 endpoint.
