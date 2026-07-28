# Port Notes: Lua → Go

Technical notes on porting the pang interpreter from Lua
(`pangea/src/pangea1/main.lua`) to Go (`main.go`).

## Size Comparison

| Metric         | Lua        | Go        | Delta            |
| :------------- | :--------- | :-------- | :--------------- |
| Lines of code  | 561        | 615       | +54 (+9.6%)      |
| Source file    | `main.lua` | `main.go` | —                |
| Built-in words | 21         | 21        | identical        |
| Binary size    | runtime    | ~2.1 MB   | Go static binary |

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
