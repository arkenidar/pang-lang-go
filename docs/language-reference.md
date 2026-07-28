# Pang Language Reference

Pang is a Polish notation (prefix notation) language. Expressions are written
with the operator/word first, followed by arguments.

```
word arg1 arg2 ...
```

## Built-in Words

### I/O

| Word             | Arity | Description                 |
| :--------------- | :---- | :-------------------------- |
| `print`          | 1     | Prints value and returns it |
| `read_text`      | 0     | Reads a line from stdin     |
| `command_prompt` | 0     | Enters interactive REPL     |

### Arithmetic

| Word                   | Arity | Description                           |
| :--------------------- | :---- | :------------------------------------ |
| `add`                  | 2     | Addition: `add 5 3` → `8`             |
| `multiply`             | 2     | Multiplication: `multiply 4 3` → `12` |
| `modulus`              | 2     | Modulo: `modulus 7 3` → `1`           |
| `lesser_than_or_equal` | 2     | ≤ comparison (legacy)                 |

### Logic & Comparison

| Word      | Arity | Description             |
| :-------- | :---- | :---------------------- |
| `true`    | 0     | Returns `true`          |
| `false`   | 0     | Returns `false`         |
| `not`     | 1     | Boolean negation        |
| `equal`   | 2     | Equality: `equal x y`   |
| `greater` | 2     | `greater a b` → `a > b` |

### Control Flow

| Word     | Arity | Description                                              |
| :------- | :---- | :------------------------------------------------------- |
| `if`     | 3     | `if condition then-branch else-branch`                   |
| `while`  | 2     | `while condition body` — loops while condition is truthy |
| `repeat` | 2     | `repeat N body` — repeats body N times                   |

### Variables

| Word           | Arity | Description                            |
| :------------- | :---- | :------------------------------------- |
| `set`          | 2     | `set "name" value` — sets a variable   |
| `get`          | 1     | `get "name"` — reads a variable        |
| `increment`    | 1     | `increment "name"` — increments by 1   |
| `variable_set` | 3     | Sets a variable in a given namespace   |
| `variable_get` | 2     | Gets a variable from a given namespace |
| `namespace`    | 0     | Returns the current call-stack frame   |

### User-Defined Words

| Word          | Arity | Description                                                  |
| :------------ | :---- | :----------------------------------------------------------- |
| `define_word` | 3     | `define_word "name" arity body` — creates a new word         |
| `argument`    | 1     | `argument N` — accesses the N-th argument inside a user word |

### Blocks & Comments

| Word         | Arity | Description                                              |
| :----------- | :---- | :------------------------------------------------------- |
| `do` / `end` | —     | Sequence block; returns the value of the last expression |
| `dont`       | 1     | Skips/ignores its argument (comment block)               |
| `?`          | 0     | Lists all defined words with their arities               |

### File I/O

| Word | Arity | Description                           |
| :--- | :---- | :------------------------------------ |
| `!`  | 1     | Includes and executes a `.words` file |

### Type Conversion

| Word        | Arity | Description                                     |
| :---------- | :---- | :---------------------------------------------- |
| `to_number` | 1     | Converts a string to a number                   |
| `string`    | 1     | Returns the raw word text at the argument index |

## Syntax

### Numbers

Numbers are parsed as `float64`: `42`, `-7`, `3.14`

### String Literals

Strings are delimited by double quotes with C-style escapes:

```
print "hello world"
print "line1\nline2"
print "tab\there"
print "quote: \"hello\""
```

### Comments / Skipping

Use `dont` to skip any expression:

```
dont print "this is ignored"
```

### Do/End Blocks

Sequential evaluation that returns the last result:

```
do
  set "x" 10
  set "y" 20
  add get "x" get "y"
end
→ 30
```

## Italian Mode

Run with `pang italian` to enable Italian keywords:

| English          | Italian             |
| :--------------- | :------------------ |
| `print`          | `stampa`            |
| `add`            | `somma`             |
| `multiply`       | `moltiplica`        |
| `if`             | `se`                |
| `while`          | `mentre`            |
| `set`            | `metti`             |
| `get`            | `prendi`            |
| `define_word`    | `definisci_parola`  |
| `argument`       | `argomento`         |
| `do` / `end`     | `fai` / `fine`      |
| `not`            | `non`               |
| `greater`        | `maggiore`          |
| `equal`          | `uguale`            |
| `modulus`        | `modulo`            |
| `string`         | `stringa`           |
| `true` / `false` | `vero` / `falso`    |
| `exit`           | `esci`              |
| `command_prompt` | `richiesta_comandi` |
| `repeat`         | `ripeti`            |
| `increment`      | `incrementa`        |
| `dont`           | `non_fare`          |

## Examples

### Hello World

```
print "Hello, world!"
```

### Fibonacci

```
define_word "fib" 1
  if equal argument 1 0
    0
    if equal argument 1 1
      1
      add fib add -1 argument 1 fib add -2 argument 1

print fib 10
→ 55
```

### Countdown

```
set "n" 5
while not equal get "n" 0 do
  print get "n"
  increment "n"
end
```

### FizzBuzz

```
define_word "multiple" 2
  equal 0 modulus argument 1 argument 2

set "i" 1
while not greater get "i" 20 do
  if multiple get "i" 15
    print "FizzBuzz"
  if multiple get "i" 3
    print "Fizz"
  if multiple get "i" 5
    print "Buzz"
  print get "i"
  set "i" add get "i" 1
end
```
