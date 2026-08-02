// Pang: Polish notation language interpreter (v028) — C++17 port
//
// Ported from main.go / main_go_structured.lua.
// Uses modern C++ features: std::variant, std::optional, std::filesystem,
// structured bindings, lambda captures, constexpr maps, string_view.
//
// Provenance chain:
//   pangea/src/pangea1/main.lua → main.go → main_go_structured.lua → cpp/*.cpp

#ifndef PANG_INTERPRETER_HPP
#define PANG_INTERPRETER_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace pang
{

    // =========================================================================
    // Value Type
    // =========================================================================
    //
    // Recursive variant design:
    //   Value uses std::shared_ptr<Namespace> to avoid circular completeness
    //   requirement. Namespace is then defined as map<string, Value>.
    //   callStack owns shared_ptrs to Namespace objects; Value shares ownership.
    //   This ensures namespace objects survive after being popped from the
    //   call stack (matching Go's map and Lua's table reference semantics).

    struct Namespace;
    using Value = std::variant<
        std::monostate,            // nil
        double,                    // number
        bool,                      // boolean
        std::string,               // string
        std::shared_ptr<Namespace> // variable namespace (shared ownership)
        >;

    struct Namespace
    {
        std::unordered_map<std::string, Value> vars;
    };

    // =========================================================================
    // Word Definition
    // =========================================================================

    using WordFunc = std::function<Value(const std::vector<int> &)>;

    struct WordDef
    {
        int arity;
        WordFunc fn;
    };

    // =========================================================================
    // Pang Interpreter
    // =========================================================================

    class PangInterpreter
    {
    public:
        PangInterpreter() = default;

        void initWordDefs();
        void run(int argc, char *argv[]);

        // Set language before initWordDefs() so definitions are registered
        // with the correct (possibly translated) names.
        void setLanguage(const std::string &lang) { language = lang; }

        // Translation helper (public so main.cpp can print translated version string).
        std::string tr(std::string_view s) const;

    private:
        // --- State ---
        std::unordered_map<std::string, WordDef> wordDefs;
        std::vector<std::string> words;
        std::vector<bool> stringLiterals;                  // true if word at same index is a string literal
        std::vector<std::shared_ptr<Namespace>> callStack; // one frame per scope (shared ownership)
        std::vector<std::string> fileDirectoryStack;
        std::string language; // "" or "italian"
        bool exitRequested = false;

        // --- Translation ---
        static const std::unordered_map<std::string, std::string> translateItalian;
        static bool truthy(const Value &v);

        // --- File path utilities ---
        static bool pathIsAbsolute(std::string_view path);
        static std::string pathDirname(std::string_view path);
        std::string resolveWordsFileName(const std::string &fileName) const;

        // --- Tokenizer ---
        void programWords(std::string_view pnProgram);
        static std::string hashbangRemove(std::string_view program);

        // --- Evaluator ---
        int phraseLength(int wordIndex) const;
        Value evaluateWord(int wordIndex);

        // --- Execution ---
        void executeProgram(std::string_view pnProgram);
        void executeWordsFile(const std::string &fileName);
        void readExecuteLoop();
    };

} // namespace pang

#endif // PANG_INTERPRETER_HPP