# REPL Differences — Original C++ Port vs. Improved

This documents the differences between the **original, plain C++ port** (a
faithful translation of `main.go`'s `readExecuteLoop()`) and the **improved
REPL** committed in `35346c7`.

## Comparison Table

| # | Aspect                    | Go (`main.go`)                 | Original C++ Port                       | Improved C++                                       |
| - | ------------------------- | ------------------------------ | --------------------------------------- | -------------------------------------------------- |
| 1 | **Prompt**                | none                           | none                                    | `>` with `std::flush`                              |
| 2 | **Empty line**            | treated as exit (`break`)      | executed → "definition not found" error | skipped silently (`continue`)                      |
| 3 | **`exit`/`esci` command** | `line == tr("exit")` → `break` | only via word-def throw `runtime_error` | handled directly: `break`                          |
| 4 | **Exception safety**      | N/A (Go panics)                | catches `std::runtime_error` only       | also catches `std::exception` (REPL never crashes) |
| 5 | **Piped-input EOF**       | exits immediately              | exits immediately                       | reopens `/dev/tty` to keep REPL alive              |

## Piped Input and `/dev/tty` Reopen

When stdin is a pipe (e.g. `echo "print 42" | ./cpp/pang`), the REPL processes
the piped commands and then reaches EOF. Instead of exiting, the REPL calls
`freopen("/dev/tty", "r", stdin)` to reconnect stdin to the real terminal,
allowing the user to continue interacting interactively.

To exit after piped input, include `exit` (or `esci` in Italian mode) in the
piped commands:

```bash
# Process piped input then stay open for interactive REPL:
echo "print 333" | ./cpp/pang

# Process piped input then exit:
echo "print 333 exit" | ./cpp/pang

# Italian mode with factorial file, piped input, then interactive REPL:
echo "stampa fattoriale 5" | ./cpp/pang italian tests/fattoriale.parole -

# Italian mode with factorial file, piped input, then exit:
echo "stampa fattoriale 5 esci" | ./cpp/pang italian tests/fattoriale.parole -
```

A second EOF (Ctrl+D) on the reopened terminal causes the REPL to exit, as the
reopen only happens once per EOF occurrence. If `/dev/tty` cannot be opened
(e.g. in a headless/CI environment), the REPL exits gracefully on the first EOF.

### Interaction with `rlwrap`

Do **not** use `rlwrap` when piping input to the interpreter. `rlwrap` requires
a real terminal and will malfunction when stdin is a pipe. The `/dev/tty` reopen
may also bypass `rlwrap`, leaving the REPL without readline features or causing
`rlwrap` to exit prematurely.

```bash
# ✅ Correct — pipe directly to pang:
echo "print 333" | ./cpp/pang

# ❌ Broken — rlwrap + pipe conflicts:
echo "stampa fattoriale 5" | rlwrap ./cpp/pang italian tests/fattoriale.parole -
```

Use `rlwrap` only for interactive sessions without piped input:

```bash
rlwrap ./cpp/pang                    # interactive REPL with readline
rlwrap ./cpp/pang italian            # Italian interactive REPL
```

## Original C++ `readExecuteLoop()` (as-is port)

```cpp
void PangInterpreter::readExecuteLoop()
{
    std::string line;
    while (!exitRequested)
    {
        if (!std::getline(std::cin, line))
        {
            break;
        }
        try
        {
            executeProgram(line);
        }
        catch (const std::runtime_error &)
        {
            if (exitRequested)
            {
                break;
            }
        }
    }
}
```

## Improved `readExecuteLoop()` (current)

```cpp
void PangInterpreter::readExecuteLoop()
{
    bool reopenedTty = false;
    std::string line;
    while (!exitRequested)
    {
        std::cout << "> " << std::flush;          // (1) prompt
        if (!std::getline(std::cin, line))
        {
            // (5) EOF — reopen /dev/tty once to continue interactive REPL
            if (!reopenedTty)
            {
                reopenedTty = true;
                std::cin.clear();
                if (freopen("/dev/tty", "r", stdin) != nullptr)
                {
                    line.clear();
                    continue;
                }
            }
            break;
        }
        reopenedTty = false;                      // reset flag on successful read
        if (line.empty())                         // (2) skip empty
        {
            continue;
        }
        if (line == "exit" || line == "esci")     // (3) direct exit
        {
            break;
        }
        try
        {
            executeProgram(line);
        }
        catch (const std::runtime_error &)
        {
            if (exitRequested) { break; }
        }
        catch (const std::exception &e)           // (4) broad catch
        {
            std::cerr << "error: " << e.what() << '\n';
        }
    }
}
```

## Notes

- The Go version already had an implicit **empty-line = exit** behavior. The
  improved C++ REPL instead **skips** empty lines (they re-prompt), which is
  more user-friendly in interactive mode.
- The Go version checked `line == tr("exit")`; the improved C++ REPL matches
  both `"exit"` and `"esci"` (Italian) directly, independent of the `tr()`
  translation mechanism, for simplicity in the REPL loop itself.
- The `std::flush` after the `>` prompt ensures it's visible even when output is
  piped or line-buffered.
- The broad `catch (const std::exception&)` makes the REPL resilient to
  unexpected errors — it prints the error and continues, rather than
  terminating.
- The `/dev/tty` reopen on EOF allows piping commands to the REPL while keeping
  it open for interactive use. Use `exit`/`esci` in the piped input to
  explicitly quit after processing.
