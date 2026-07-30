// Pang: Polish notation language interpreter (v028) — C++17 port
// Implementation file — see interpreter.hpp for interface and architecture notes.

#include "interpreter.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace pang
{

    // =========================================================================
    // Italian Translation Table (static const)
    // =========================================================================

    const std::unordered_map<std::string, std::string> PangInterpreter::translateItalian = {
        {"pang version: ", "pang versione: "},
        {"exit", "esci"},
        {"print", "stampa"},
        {"define_word", "definisci_parola"},
        {"multiply", "moltiplica"},
        {"argument", "argomento"},
        {"do", "fai"},
        {"end", "fine"},
        {"set", "metti"},
        {"get", "prendi"},
        {"variable_set", "metti_variabile"},
        {"variable_get", "prendi_variabile"},
        {"caller_set", "metti_chiamante"},
        {"caller_get", "prendi_chiamante"},
        {"while", "mentre"},
        {"not", "non"},
        {"greater", "maggiore"},
        {"if", "se"},
        {"equal", "uguale"},
        {"modulus", "modulo"},
        {"string", "stringa"},
        {"add", "somma"},
        {"true", "vero"},
        {"false", "falso"},
        {"dont", "non_fare"},
        {"word:", "parola:"},
        {" definition not found", " definizione non trovata"},
        {"command_prompt", "richiesta_comandi"},
        {"read_text", "leggi_testo"},
        {"to_number", "numero_da_testo"},
        {"repeat", "ripeti"},
        {"increment", "incrementa"},
        {"and", "e"},
        {"or", "o"},
    };

    // =========================================================================
    // Translation Helper
    // =========================================================================

    std::string PangInterpreter::tr(std::string_view s) const
    {
        if (language == "italian")
        {
            std::string key{s};
            auto it = translateItalian.find(key);
            if (it != translateItalian.end())
            {
                return it->second;
            }
            std::cerr << "can't translate: " << s << '\n';
            return key;
        }
        return std::string{s};
    }

    bool PangInterpreter::truthy(const Value &v)
    {
        if (std::holds_alternative<std::monostate>(v))
        {
            return false;
        }
        if (auto *b = std::get_if<bool>(&v))
        {
            return *b;
        }
        return true;
    }

    // =========================================================================
    // File Path Utilities (using C++17 std::filesystem)
    // =========================================================================

    bool PangInterpreter::pathIsAbsolute(std::string_view path)
    {
        if (!path.empty() && path[0] == '/')
        {
            return true;
        }
        if (path.size() >= 2 && path[1] == ':' &&
            ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
        {
            return true;
        }
        return false;
    }

    std::string PangInterpreter::pathDirname(std::string_view path)
    {
        namespace fs = std::filesystem;
        fs::path p{std::string{path}};
        auto parent = p.parent_path();
        if (parent.empty())
        {
            return ".";
        }
        return parent.string();
    }

    std::string PangInterpreter::resolveWordsFileName(const std::string &fileName) const
    {
        if (pathIsAbsolute(fileName))
        {
            return fileName;
        }
        if (fileDirectoryStack.empty())
        {
            return fileName;
        }
        return fileDirectoryStack.back() + "/" + fileName;
    }

    // =========================================================================
    // Tokenizer / Lexer (port of programWords)
    // =========================================================================

    void PangInterpreter::programWords(std::string_view pnProgram)
    {
        std::string token;
        std::string quoted;
        bool inString = false;
        bool escaping = false;

        auto flushToken = [&]()
        {
            if (!token.empty())
            {
                words.push_back(token);
                stringLiterals.push_back(false);
                token.clear();
            }
        };

        auto flushQuoted = [&]()
        {
            words.push_back(quoted);
            stringLiterals.push_back(true);
            quoted.clear();
        };

        for (char ch : pnProgram)
        {
            std::string c(1, ch);
            if (inString)
            {
                if (escaping)
                {
                    if (ch == '"')
                    {
                        quoted += '"';
                    }
                    else if (ch == '\\')
                    {
                        quoted += '\\';
                    }
                    else if (ch == 'n')
                    {
                        quoted += '\n';
                    }
                    else if (ch == 't')
                    {
                        quoted += '\t';
                    }
                    else
                    {
                        std::cerr << "invalid escape sequence: \\" << c << '\n';
                    }
                    escaping = false;
                }
                else if (ch == '\\')
                {
                    escaping = true;
                }
                else if (ch == '"')
                {
                    flushQuoted();
                    inString = false;
                }
                else
                {
                    quoted += ch;
                }
            }
            else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            {
                flushToken();
            }
            else if (ch == '"')
            {
                flushToken();
                inString = true;
            }
            else
            {
                token += ch;
            }
        }

        if (escaping)
        {
            std::cerr << "unterminated escape sequence in string literal\n";
        }
        if (inString)
        {
            std::cerr << "unterminated string literal\n";
        }

        flushToken();
    }

    std::string PangInterpreter::hashbangRemove(std::string_view program)
    {
        if (!program.empty() && program[0] == '#')
        {
            auto pos = program.find('\n');
            if (pos != std::string::npos)
            {
                return std::string{program.substr(pos + 1)};
            }
            return "";
        }
        return std::string{program};
    }

    // =========================================================================
    // Core Evaluator
    // =========================================================================

    int PangInterpreter::phraseLength(int wordIndex) const
    {
        int idx = wordIndex - 1;
        const std::string &word = words[idx];
        int length = 1;

        // String literals always have length 1.
        if (stringLiterals[idx])
        {
            return 1;
        }

        // do ... end blocks
        if (word == tr("do"))
        {
            while (true)
            {
                int lookAhead = wordIndex + length - 1;
                if (lookAhead >= static_cast<int>(words.size()) || words[lookAhead] == tr("end"))
                {
                    return length + 1;
                }
                length += phraseLength(wordIndex + length);
            }
        }

        // Numbers have length 1.
        {
            const char *s = word.c_str();
            char *end = nullptr;
            std::strtod(s, &end);
            if (end != s && *end == '\0')
            {
                return 1;
            }
        }

        auto it = wordDefs.find(word);
        if (it == wordDefs.end())
        {
            return 1;
        }

        for (int i = 0; i < it->second.arity; ++i)
        {
            length += phraseLength(wordIndex + length);
        }
        return length;
    }

    Value PangInterpreter::evaluateWord(int wordIndex)
    {
        int idx = wordIndex - 1;
        const std::string &word = words[idx];

        // String literals return their raw value.
        if (stringLiterals[idx])
        {
            return word;
        }

        // Number literals.
        {
            const char *s = word.c_str();
            char *end = nullptr;
            double val = std::strtod(s, &end);
            if (end != s && *end == '\0')
            {
                return val;
            }
        }

        // do ... end blocks
        if (word == tr("do"))
        {
            int doWordIndex = wordIndex + 1;
            Value evaluated;
            while (doWordIndex - 1 < static_cast<int>(words.size()) && words[doWordIndex - 1] != tr("end"))
            {
                evaluated = evaluateWord(doWordIndex);
                doWordIndex += phraseLength(doWordIndex);
            }
            return evaluated;
        }

        auto it = wordDefs.find(word);
        if (it == wordDefs.end())
        {
            std::cout << tr("word:") << '"' << word << '"' << tr(" definition not found") << '\n';
            return {};
        }

        const WordDef &def = it->second;
        std::vector<int> args;
        args.reserve(def.arity);
        int argWordIndex = wordIndex + 1;
        for (int i = 0; i < def.arity; ++i)
        {
            args.push_back(argWordIndex);
            argWordIndex += phraseLength(argWordIndex);
        }

        return def.fn(args);
    }

    // =========================================================================
    // Program Execution
    // =========================================================================

    void PangInterpreter::executeProgram(std::string_view pnProgram)
    {
        std::string wrapped;
        wrapped.reserve(tr("do").size() + 1 + pnProgram.size() + 1 + tr("end").size());
        wrapped += tr("do");
        wrapped += ' ';
        wrapped += pnProgram;
        wrapped += ' ';
        wrapped += tr("end");

        std::size_t wordsBefore = words.size();
        programWords(wrapped);

        if (words.size() == wordsBefore)
        {
            return;
        }

        try
        {
            evaluateWord(static_cast<int>(wordsBefore) + 1);
        }
        catch (const std::runtime_error &)
        {
            if (exitRequested)
            {
                throw;
            }
        }
    }

    void PangInterpreter::executeWordsFile(const std::string &fileName)
    {
        std::string resolvedFileName = resolveWordsFileName(fileName);

        auto readFile = [](const std::string &path) -> std::optional<std::string>
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                return std::nullopt;
            }
            std::ostringstream oss;
            oss << file.rdbuf();
            return oss.str();
        };

        auto content = readFile(resolvedFileName);

        // Fallback to original name for compatibility.
        if (!content && resolvedFileName != fileName)
        {
            content = readFile(fileName);
            if (content)
            {
                resolvedFileName = fileName;
            }
        }

        if (!content)
        {
            std::cerr << "cannot open words file: " << fileName << '\n';
            return;
        }

        std::string program = hashbangRemove(*content);

        fileDirectoryStack.push_back(pathDirname(resolvedFileName));
        try
        {
            executeProgram(program);
        }
        catch (const std::runtime_error &)
        {
            fileDirectoryStack.pop_back();
            if (exitRequested)
            {
                throw;
            }
        }
        fileDirectoryStack.pop_back();
    }

    void PangInterpreter::readExecuteLoop()
    {
        bool reopenedTty = false;
        std::string line;
        while (!exitRequested)
        {
            std::cout << "> " << std::flush;
            if (!std::getline(std::cin, line))
            {
                // EOF on stdin — try to reopen from the terminal so the
                // user can continue the REPL interactively.  Only do this
                // once; a second EOF means the user really wants to quit.
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

            // Reset the flag when we successfully read a line (user is
            // interacting — give them a fresh EOF counter next time).
            reopenedTty = false;

            // Skip empty input.
            if (line.empty())
            {
                continue;
            }

            // Check for direct "exit"/"esci" command to quit REPL.
            if (line == "exit" || line == "esci")
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
            catch (const std::exception &e)
            {
                std::cerr << "error: " << e.what() << '\n';
            }
        }
    }

    void PangInterpreter::run(int argc, char *argv[])
    {
        // Parse arguments — skip "italian" (language already set before run).
        std::string filename;
        bool repl = false;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg{argv[i]};
            if (arg == "italian")
            {
                // Already handled — skip.
            }
            else if (arg == "-" || arg == "--repl")
            {
                repl = true;
            }
            else
            {
                filename = arg;
            }
        }

        if (!filename.empty())
        {
            try
            {
                executeWordsFile(filename);
            }
            catch (const std::runtime_error &)
            {
                // exit was requested, return cleanly
            }
        }

        if (!exitRequested && (repl || filename.empty()))
        {
            readExecuteLoop();
        }
    }

    // =========================================================================
    // Built-in Word Definitions (centralized — mirrors main.go wordDefsInit)
    // =========================================================================

    void PangInterpreter::initWordDefs()
    {
        // Initialize call stack with one empty frame.
        callStack.emplace_back();

        // print <printable>
        wordDefs[tr("print")] = {1, [this](const std::vector<int> &args) -> Value
                                 {
                                     Value val = evaluateWord(args[0]);
                                     std::visit([](const auto &v)
                                                {
                                                    using T = std::decay_t<decltype(v)>;
                                                    if constexpr (std::is_same_v<T, std::monostate>)
                                                    {
                                                        std::cout << "nil\n";
                                                    }
                                                    else if constexpr (std::is_same_v<T, Namespace *>)
                                                    {
                                                        std::cout << "[namespace]\n";
                                                    }
                                                    else
                                                    {
                                                        std::cout << v << '\n';
                                                    } },
                                                val);
                                     return val;
                                 }};

        // read_text
        wordDefs[tr("read_text")] = {0, [](const std::vector<int> &) -> Value
                                     {
                                         std::string text;
                                         if (!std::getline(std::cin, text))
                                         {
                                             return std::string{""};
                                         }
                                         return text;
                                     }};

        // to_number <text>
        wordDefs[tr("to_number")] = {1, [this](const std::vector<int> &args) -> Value
                                     {
                                         Value val = evaluateWord(args[0]);
                                         if (auto *s = std::get_if<std::string>(&val))
                                         {
                                             char *end = nullptr;
                                             double n = std::strtod(s->c_str(), &end);
                                             if (end != s->c_str() && *end == '\0')
                                             {
                                                 return n;
                                             }
                                             return 0.0;
                                         }
                                         return val;
                                     }};

        // add <number> <number>
        wordDefs[tr("add")] = {2, [this](const std::vector<int> &args) -> Value
                               {
                                   Value a = evaluateWord(args[0]);
                                   Value b = evaluateWord(args[1]);
                                   return std::get<double>(a) + std::get<double>(b);
                               }};

        // multiply <number> <number>
        wordDefs[tr("multiply")] = {2, [this](const std::vector<int> &args) -> Value
                                    {
                                        Value a = evaluateWord(args[0]);
                                        Value b = evaluateWord(args[1]);
                                        return std::get<double>(a) * std::get<double>(b);
                                    }};

        // true
        wordDefs[tr("true")] = {0, [](const std::vector<int> &) -> Value
                                { return true; }};

        // false
        wordDefs[tr("false")] = {0, [](const std::vector<int> &) -> Value
                                 { return false; }};

        // if <condition> <if true> <if false>
        wordDefs[tr("if")] = {3, [this](const std::vector<int> &args) -> Value
                              {
                                  if (truthy(evaluateWord(args[0])))
                                  {
                                      return evaluateWord(args[1]);
                                  }
                                  return evaluateWord(args[2]);
                              }};

        // while <condition> <do while true>
        wordDefs[tr("while")] = {2, [this](const std::vector<int> &args) -> Value
                                 {
                                     while (truthy(evaluateWord(args[0])))
                                     {
                                         Value result = evaluateWord(args[1]);
                                         if (auto *s = std::get_if<std::string>(&result))
                                         {
                                             if (*s == "break")
                                             {
                                                 break;
                                             }
                                         }
                                     }
                                     return {};
                                 }};

        // repeat <times_count> <deferred_code>
        wordDefs[tr("repeat")] = {2, [this](const std::vector<int> &args) -> Value
                                  {
                                      Value total = evaluateWord(args[0]);
                                      auto *n = std::get_if<double>(&total);
                                      if (!n)
                                      {
                                          return {};
                                      }
                                      Value result;
                                      for (int64_t i = 0; i < static_cast<int64_t>(*n); ++i)
                                      {
                                          result = evaluateWord(args[1]);
                                      }
                                      return result;
                                  }};

        // not <boolean>
        wordDefs[tr("not")] = {1, [this](const std::vector<int> &args) -> Value
                               { return !truthy(evaluateWord(args[0])); }};

        // and <boolean> <boolean>
        wordDefs[tr("and")] = {2, [this](const std::vector<int> &args) -> Value
                               { return truthy(evaluateWord(args[0])) && truthy(evaluateWord(args[1])); }};

        // or <boolean> <boolean>
        wordDefs[tr("or")] = {2, [this](const std::vector<int> &args) -> Value
                              { return truthy(evaluateWord(args[0])) || truthy(evaluateWord(args[1])); }};

        // equal <first> <second>
        wordDefs[tr("equal")] = {2, [this](const std::vector<int> &args) -> Value
                                 {
                                     Value a = evaluateWord(args[0]);
                                     Value b = evaluateWord(args[1]);

                                     // Compare structurally — both must be same alternative and equal value.
                                     if (a.index() != b.index())
                                     {
                                         return false;
                                     }

                                     if (std::holds_alternative<std::monostate>(a))
                                     {
                                         return true; // both nil
                                     }
                                     if (auto *da = std::get_if<double>(&a))
                                     {
                                         return *da == std::get<double>(b);
                                     }
                                     if (auto *ba = std::get_if<bool>(&a))
                                     {
                                         return *ba == std::get<bool>(b);
                                     }
                                     if (auto *sa = std::get_if<std::string>(&a))
                                     {
                                         return *sa == std::get<std::string>(b);
                                     }
                                     // Namespace equality — compare pointers (same object)
                                     return std::get<Namespace *>(a) == std::get<Namespace *>(b);
                                 }};

        // set <variable name> <value>
        wordDefs[tr("set")] = {2, [this](const std::vector<int> &args) -> Value
                               {
                                   Namespace &vars = callStack.back();
                                   Value nameVal = evaluateWord(args[0]);
                                   std::string varName = std::get<std::string>(nameVal);
                                   vars.vars[varName] = evaluateWord(args[1]);
                                   return {};
                               }};

        // get <variable name>
        wordDefs[tr("get")] = {1, [this](const std::vector<int> &args) -> Value
                               {
                                   Namespace &vars = callStack.back();
                                   Value nameVal = evaluateWord(args[0]);
                                   std::string varName = std::get<std::string>(nameVal);
                                   auto it = vars.vars.find(varName);
                                   if (it == vars.vars.end())
                                   {
                                       std::cout << "nil returning from get_function()\n";
                                       return {};
                                   }
                                   return it->second;
                               }};

        // variable_set <namespace> <variable name> <value>
        wordDefs[tr("variable_set")] = {3, [this](const std::vector<int> &args) -> Value
                                        {
                                            Value nsVal = evaluateWord(args[0]);
                                            Namespace *ns = std::get<Namespace *>(nsVal);
                                            Value nameVal = evaluateWord(args[1]);
                                            std::string varName = std::get<std::string>(nameVal);
                                            ns->vars[varName] = evaluateWord(args[2]);
                                            return {};
                                        }};

        // variable_get <namespace> <variable name>
        wordDefs[tr("variable_get")] = {2, [this](const std::vector<int> &args) -> Value
                                        {
                                            Value nsVal = evaluateWord(args[0]);
                                            Namespace *ns = std::get<Namespace *>(nsVal);
                                            Value nameVal = evaluateWord(args[1]);
                                            std::string varName = std::get<std::string>(nameVal);
                                            auto it = ns->vars.find(varName);
                                            if (it == ns->vars.end())
                                            {
                                                return {};
                                            }
                                            return it->second;
                                        }};

        // namespace — returns current call stack frame (as pointer)
        wordDefs["namespace"] = {0, [this](const std::vector<int> &) -> Value
                                 { return &callStack.back(); }};

        // modulus <dividend> <divisor>
        wordDefs[tr("modulus")] = {2, [this](const std::vector<int> &args) -> Value
                                   {
                                       Value aVal = evaluateWord(args[0]);
                                       Value bVal = evaluateWord(args[1]);
                                       int64_t a = static_cast<int64_t>(std::get<double>(aVal));
                                       int64_t b = static_cast<int64_t>(std::get<double>(bVal));
                                       return static_cast<double>(a % b);
                                   }};

        // greater <lesser> <greater>
        wordDefs[tr("greater")] = {2, [this](const std::vector<int> &args) -> Value
                                   {
                                       Value aVal = evaluateWord(args[0]);
                                       Value bVal = evaluateWord(args[1]);
                                       return std::get<double>(aVal) > std::get<double>(bVal);
                                   }};

        // ? — list word definitions
        wordDefs["?"] = {0, [this](const std::vector<int> &) -> Value
                         {
                             for (const auto &[word, wd] : wordDefs)
                             {
                                 std::cout << word << '<' << wd.arity << " ";
                             }
                             std::cout << '\n';
                             return {};
                         }};

        // ! — execute_words_file <filename>
        wordDefs["!"] = {1, [this](const std::vector<int> &args) -> Value
                         {
                             std::string fileName = words[args[0] - 1];
                             executeWordsFile(fileName);
                             return {};
                         }};

        // dont <skip this>
        wordDefs[tr("dont")] = {1, [](const std::vector<int> &) -> Value
                                { return {}; }};

        // define_word <name> <arity> <action>
        wordDefs[tr("define_word")] = {3, [this](const std::vector<int> &args) -> Value
                                       {
                                           Value nameVal = evaluateWord(args[0]);
                                           std::string name = std::get<std::string>(nameVal);
                                           Value arityVal = evaluateWord(args[1]);
                                           int arity = static_cast<int>(std::get<double>(arityVal));
                                           int bodyIndex = args[2];

                                           WordDef wd;
                                           wd.arity = arity;
                                           wd.fn = [this, bodyIndex](const std::vector<int> &wordArgs) -> Value
                                           {
                                               Namespace valueArgs;
                                               for (std::size_t i = 0; i < wordArgs.size(); ++i)
                                               {
                                                   valueArgs.vars[std::to_string(i + 1)] = evaluateWord(wordArgs[i]);
                                               }
                                               callStack.push_back(std::move(valueArgs));
                                               Value result = evaluateWord(bodyIndex);
                                               callStack.pop_back();
                                               return result;
                                           };
                                           wordDefs[name] = std::move(wd);
                                           return {};
                                       }};

        // argument <argument index>
        wordDefs[tr("argument")] = {1, [this](const std::vector<int> &args) -> Value
                                    {
                                        Namespace &frame = callStack.back();
                                        Value idxVal = evaluateWord(args[0]);
                                        std::string argIndex = std::to_string(static_cast<int>(std::get<double>(idxVal)));
                                        auto it = frame.vars.find(argIndex);
                                        if (it == frame.vars.end())
                                        {
                                            return {};
                                        }
                                        return it->second;
                                    }};

        // command_prompt — REPL
        wordDefs[tr("command_prompt")] = {0, [this](const std::vector<int> &) -> Value
                                          {
                                              readExecuteLoop();
                                              return {};
                                          }};

        // exit — terminate the interpreter cleanly (also "esci" in Italian)
        wordDefs[tr("exit")] = {0, [this](const std::vector<int> &) -> Value
                                {
                                    exitRequested = true;
                                    throw std::runtime_error("exit");
                                }};

        // increment <variable name>
        wordDefs[tr("increment")] = {1, [this](const std::vector<int> &args) -> Value
                                     {
                                         Namespace &vars = callStack.back();
                                         Value nameVal = evaluateWord(args[0]);
                                         std::string varName = std::get<std::string>(nameVal);
                                         auto it = vars.vars.find(varName);
                                         double val = 0.0;
                                         if (it != vars.vars.end())
                                         {
                                             if (auto *d = std::get_if<double>(&it->second))
                                             {
                                                 val = *d;
                                             }
                                         }
                                         val += 1.0;
                                         vars.vars[varName] = val;
                                         return val;
                                     }};
    }

} // namespace pang