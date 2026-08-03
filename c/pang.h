// Pang: Polish notation language interpreter (v028) — C11 port
//
// Ported from cpp/interpreter.{hpp,cpp} (C++17) to plain C.
// Uses tagged union for Value type, setjmp/longjmp for exit flow,
// and custom growable arrays + linear search for all containers.
//
// Provenance chain:
//   pangea/src/pangea1/main.lua → main.go → main_go_structured.lua
//   → cpp/*.cpp → c/*.c

#ifndef PANG_H
#define PANG_H

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // =============================================================================
    // Value Type (tagged union)
    // =============================================================================

    enum ValueType
    {
        VAL_NIL,
        VAL_NUM,
        VAL_BOOL,
        VAL_STR,
        VAL_NS,
    };
    typedef enum ValueType ValueType;

    struct Namespace;

    typedef struct Value
    {
        ValueType type;
        union
        {
            double num;
            bool boolean;
            char *str;            // owned heap string
            struct Namespace *ns; // pointer into callStack (not owned)
        };
    } Value;

    // =============================================================================
    // Namespace (variable frame)
    // =============================================================================

    typedef struct Namespace
    {
        char **keys; // variable names
        Value *vals; // variable values
        int count;
        int cap;
    } Namespace;

    // =============================================================================
    // Word Definition
    // =============================================================================

    struct PangCtx; // forward decl

    typedef Value (*WordFn)(struct PangCtx *ctx, const int *args);

    typedef struct WordDef
    {
        int arity;
        WordFn fn;
        // For define_word closures: stores the body word index.
        int closure_body_index;
        bool is_closure;
    } WordDef;

    // =============================================================================
    // WordDefs Entry (for the wordDefs map)
    // =============================================================================

    typedef struct
    {
        char *key;
        WordDef def;
    } WordDefsEntry;

    // =============================================================================
    // Dynamic String Array
    // =============================================================================

    typedef struct
    {
        char **data;
        int len;
        int cap;
    } StrVec;

    // =============================================================================
    // Dynamic Bool Array
    // =============================================================================

    typedef struct
    {
        bool *data;
        int len;
        int cap;
    } BoolVec;

    // =============================================================================
    // Dynamic Namespace Pointer Array (callStack)
    // =============================================================================

    typedef struct
    {
        Namespace **data;
        int len;
        int cap;
    } NSVec;

    // =============================================================================
    // Pang Context — all interpreter state
    // =============================================================================

    typedef struct PangCtx
    {
        // Word definitions
        WordDefsEntry *word_defs_entries;
        int word_defs_count;
        int word_defs_cap;

        // Tokenized words + string-literal flags
        StrVec words;
        BoolVec string_literals;

        // Call stack (one frame per scope)
        NSVec call_stack;

        // File directory stack for relative include resolution
        StrVec file_directory_stack;

        // Language ("", "italian")
        char *language;

        // Exit flag + jump buffer for longjmp-based exit
        bool exit_requested;
        jmp_buf exit_jmpbuf;
    } PangCtx;

    // =============================================================================
    // API
    // =============================================================================

    void pang_init(PangCtx *ctx);
    void pang_destroy(PangCtx *ctx);
    void pang_set_language(PangCtx *ctx, const char *lang);
    void pang_run(PangCtx *ctx, int argc, char *argv[]);

    // Exposed for testing
    const char *pang_tr(PangCtx *ctx, const char *s);
    bool pang_truthy(Value v);

#ifdef __cplusplus
}
#endif

#endif // PANG_H