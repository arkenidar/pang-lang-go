# Port Notes: Lua → Go

Technical notes on porting the pang interpreter from Lua
(`pangea/src/pangea1/main.lua`) to Go (`main.go`).

## Provenance: Source File Relationships

The five related files and how they connect:

```
pangea/src/pangea1/main.lua  (original canonical v028, 561 LOC)
  |
  |--- main.go                 (Go port, structurally reorganized, 615 LOC)
  |      |
  |      `--- main_go_structured.lua  (reverse-port — Lua mirror of main.go's
  |                                     section ordering, for 1:1 correspondence)
  |
  |--- pangea/ark/lua/latest.lua       (near-canonical v028, 590 LOC;
  |                                     subset of main.lua — no file_directory_stack,
  |                                     no hashbang_remove, no repeat/read_text/to_number)
  |
  `--- pangea/ark/lua/pang-028.lua     (historical snapshot, 506 LOC;
                                        legacy `:` string syntax, no string_literals array)
```

| File                          | LOC | Role                                                              |
| :---------------------------- | :-- | :---------------------------------------------------------------- |
| `pangea/src/pangea1/main.lua` | 561 | **Canonical v028** — original source for `main.go`                |
| `main.go`                     | 615 | **Go port** — reorganized for Go's compilation model              |
| `main_go_structured.lua`      | 564 | **Structural mirror** — Lua back-port matching `main.go`'s layout |
| `pangea/ark/lua/latest.lua`   | 590 | Near-canonical v028, subset of `main.lua`                         |
| `pangea/ark/lua/pang-028.lua` | 506 | Historical snapshot with legacy `:` syntax                        |

### Structural Reorganization in `main.go`

The original `main.lua` scatters `word_definitions[x] = {n, f}` assignments
throughout the file (some before `phrase_length`, some after `evaluate_word`,
some at the very bottom). The Go port gathers all definitions into a single
`wordDefsInit()` function and organizes sections as:

1. Types → Global State → Italian Translation → File Path Utilities
2. Centralized `wordDefsInit()` (all 21 built-in words)
3. Tokenizer (`programWords`, `hashbangRemove`)
4. Core Evaluator (`phraseLength`, `evaluateWord`)
5. Program Execution (`executeProgram`, `executeWordsFile`, REPL)
6. `main()`

`main_go_structured.lua` follows this exact layout so that every line of
`main.go` has a direct Lua counterpart.

## Size Comparison

| Metric         | main.lua | main.go | main_go_structured.lua | Delta (main.lua → main.go) |
| :------------- | :------- | :------ | :--------------------- | :------------------------- |
| Lines of code  | 561      | 615     | 564                    | +54 (+9.6%)                |
| Built-in words | 21       | 21      | 21                     | identical                  |
| Binary size    | runtime  | ~2.1 MB | runtime                | Go static binary           |

## Key Porting Decisions

### 1. Dynamic Typing → `interface{}`

Lua uses implicit dynamic typing throughout. Go requires explicit `interface{}`
for polymorphic values with type assertions:

```go
// Lua: local first_number = evaluate_word(arguments[1]) + evaluate_word(arguments[2])
fn(args[0]).(float64) + fn(args[1]).(float64)
```

### 2. Table Functions → Struct + Map

Lua uses tables in a tuple-like pattern: `{arity, function}`. Go uses a struct:

```go
type WordDef struct {
    Arity int
    Fn    WordFunc
}
wordDefs = map[string]WordDef{}
```

### 3. 1-Indexed → 0-Indexed Bridging

The Lua interpreter uses 1-based indexing throughout (Lua convention). The Go
port maintains 1-based `wordIndex` parameters internally with
`idx := wordIndex - 1` conversion at boundaries. This minimizes logic changes
from the original.

### 4. Character-Level Tokenizer

The canonical tokenizer handles string literals character-by-character with
escape sequences. This was ported faithfully:

```go
func programWords(pnProgram string) {
    // Character-by-character loop with inString/escaping flags
    // Supports: \n, \t, \\, \"
}
```

### 5. Call Stack as `[]map[string]interface{}`

Lua's `call_stack = {{}}` (table of tables) maps to Go's slice of maps:

```go
var callStack = []map[string]interface{}{{}}
```

Frames are pushed/popped during `define_word` function calls and `argument`
resolution steps.

### 6. File Path Resolution

The Lua version builds a `file_directory_stack` for relative include resolution.
Go port uses `filepath.Dir` and manual path joining:

```go
func resolveWordsFileName(fileName string) string {
    if pathIsAbsolute(fileName) { return fileName }
    if len(fileDirectoryStack) == 0 { return fileName }
    return fileDirectoryStack[len(fileDirectoryStack)-1] + "/" + fileName
}
```

### 7. Italian Translation

Translation is handled by a `tr()` function wrapping a `map[string]string`
lookup, identical to the Lua version's table-based approach. The `italian` flag
set via CLI argument toggles all keywords.

### 8. REPL

Lua's `io.read()` REPL maps to Go's `bufio.Scanner`:

```go
scanner := bufio.NewScanner(os.Stdin)
for scanner.Scan() {
    executeProgram(scanner.Text())
}
```

## Differences from Original

1. **Exit behavior:** Lua version exits on empty line or `"exit"`. Go port exits
   on `"exit"` or EOF, not empty line (matches REPL UX better).
2. **Error messages:** Go prints to stderr for tokenizer errors (unterminated
   strings, invalid escapes). Lua uses `error()` which aborts completely.
3. **String literals in `set`/`get`:** The canonical v028 uses
   `evaluateWord(args[0]).(string)` for variable names, requiring quoted strings
   (`set "x" 1`). The ark snapshots (pang-004 through pang-010) use raw word
   text (`words[args[0]-1]`).
4. **Number representation:** All numbers are `float64`, matching Lua's single
   number type. Modulo uses `int64` cast.
5. **`break` in while:** Uses string sentinel `"break"` return value, matching
   Lua behavior. The v029 prerelease uses a reserved table instead.

## Verification

All test outputs match the Lua original byte-for-byte:

- `factorial 3` = `6`
- `fizzbuzz 1..20` = correct Fizz/Buzz/FizzBuzz
- Italian `fattoriale 4` = `24`
- Italian `fizzbuzz` = correct Italian annotations
- File includes (`!`) = works with relative paths
- `define_word` recursion = factorial/square both work
- `repeat` = loops correct number of times
- `command_prompt` = REPL functions identically
