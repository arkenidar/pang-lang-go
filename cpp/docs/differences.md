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
    std::string line;
    while (!exitRequested)
    {
        std::cout << "> " << std::flush;          // (1) prompt
        if (!std::getline(std::cin, line))
        {
            break;
        }
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
