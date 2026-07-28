# pang-lang-go

**Pang** is a [Polish notation](https://en.wikipedia.org/wiki/Polish_notation)
(prefix notation) programming language — originally written in Lua, now ported
to Go.

> _pang → polish notation language_

```
print "Hello, world!"
print add 5 6    →  11
print multiply 3 4    →  12
```

## Quick Start

```bash
go build -o pang main.go

# Run a program file
./pang tests/fizzbuzz.words

# Interactive REPL
./pang -

# Italian mode
./pang italian tests/fattoriale.parole
```

## Language Features

- **Prefix notation** — operators precede their operands: `add 1 2`,
  `if condition then else`
- **Built-in words** — `print`, `add`, `multiply`, `if`, `while`, `repeat`,
  `set`/`get`, `define_word`/`argument`, and more
- **User-defined words** — first-class, recursive:
  `define_word "factorial" 1 if equal 0 argument 1 1 multiply argument 1 factorial add -1 argument 1`
- **Italian translation** — `./pang italian` swaps all keywords
- **File includes** — `! filename.words` with relative path resolution
- **String literals** — `"hello\nworld"` with escape sequences
- **REPL** — interactive read-eval-print loop

## Project Structure

```
pang-lang-go/
├── README.md
├── go.mod
├── main.go              ← canonical v028 interpreter (615 LOC)
├── pang                 ← compiled binary
├── ark/                 ← evolutionary snapshot archive
│   ├── pang-000.go      (phrase_length)
│   ├── pang-001.go      (+ do/end)
│   ├── pang-002.go      (+ evaluate_word)
│   ├── pang-003.go      (+ if/true/false)
│   ├── pang-004.go      (+ FizzBuzz-capable: while, variables)
│   ├── pang-006.go      (+ file I/O, REPL)
│   └── pang-010.go      (+ define_word, call_stack)
├── docs/
│   ├── language-reference.md
│   ├── ark-snapshots.md
│   └── port-notes.md
└── tests/
    ├── *.words           (English examples)
    └── *.parole          (Italian examples)
```

## Line Count

| Implementation    | Language | LOC |
| :---------------- | :------- | :-- |
| Original (pangea) | Lua      | 561 |
| Go port           | Go       | 615 |

## Documentation

→ [Docs index](docs/index.md) — Language reference, ark snapshots, port notes

## License

MIT (see original [pangea](https://github.com/arkenidar/pangea) repository).
