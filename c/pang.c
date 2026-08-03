// Pang: Polish notation language interpreter (v028) — C11 port
// Implementation — see pang.h for interface and architecture notes.

#include "pang.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Utility macros
// =============================================================================

#define PANG_MAX(a, b) ((a) > (b) ? (a) : (b))

static void *pang_malloc(size_t n)
{
    void *p = malloc(n);
    if (!p)
    {
        fprintf(stderr, "pang: out of memory\n");
        abort();
    }
    return p;
}

static void *pang_realloc(void *p, size_t n)
{
    void *np = realloc(p, n);
    if (!np)
    {
        fprintf(stderr, "pang: out of memory\n");
        abort();
    }
    return np;
}

static char *pang_strdup(const char *s)
{
    size_t len = strlen(s);
    char *d = (char *)pang_malloc(len + 1);
    memcpy(d, s, len + 1);
    return d;
}

// =============================================================================
// Dynamic array helpers
// =============================================================================

#define GROW_MIN_CAP 8

static void strvec_push(StrVec *v, const char *s)
{
    if (v->len >= v->cap)
    {
        v->cap = v->cap < GROW_MIN_CAP ? GROW_MIN_CAP : v->cap * 2;
        v->data = (char **)pang_realloc(v->data, (size_t)v->cap * sizeof(char *));
    }
    v->data[v->len++] = pang_strdup(s);
}

static void strvec_free(StrVec *v)
{
    for (int i = 0; i < v->len; i++)
        free(v->data[i]);
    free(v->data);
    v->data = NULL;
    v->len = v->cap = 0;
}

static void boolvec_push(BoolVec *v, bool b)
{
    if (v->len >= v->cap)
    {
        v->cap = v->cap < GROW_MIN_CAP ? GROW_MIN_CAP : v->cap * 2;
        v->data = (bool *)pang_realloc(v->data, (size_t)v->cap * sizeof(bool));
    }
    v->data[v->len++] = b;
}

static void nsvec_push(NSVec *v, Namespace *ns)
{
    if (v->len >= v->cap)
    {
        v->cap = v->cap < GROW_MIN_CAP ? GROW_MIN_CAP : v->cap * 2;
        v->data = (Namespace **)pang_realloc(v->data, (size_t)v->cap * sizeof(Namespace *));
    }
    v->data[v->len++] = ns;
}

static Namespace *nsvec_pop(NSVec *v)
{
    if (v->len == 0)
        return NULL;
    return v->data[--v->len];
}

// =============================================================================
// Namespace helpers
// =============================================================================

static Namespace *namespace_new(void)
{
    Namespace *ns = (Namespace *)pang_malloc(sizeof(Namespace));
    ns->keys = NULL;
    ns->vals = NULL;
    ns->count = 0;
    ns->cap = 0;
    return ns;
}

static void namespace_free(Namespace *ns)
{
    if (!ns)
        return;
    for (int i = 0; i < ns->count; i++)
    {
        free(ns->keys[i]);
        if (ns->vals[i].type == VAL_STR)
            free(ns->vals[i].str);
    }
    free(ns->keys);
    free(ns->vals);
    free(ns);
}

// Get value from namespace by key. Returns false if not found.
static bool namespace_get(Namespace *ns, const char *key, Value *out)
{
    for (int i = 0; i < ns->count; i++)
    {
        if (strcmp(ns->keys[i], key) == 0)
        {
            *out = ns->vals[i];
            return true;
        }
    }
    out->type = VAL_NIL;
    return false;
}

// Set value in namespace by key. Frees old value if key already exists.
static void namespace_set(Namespace *ns, const char *key, Value val)
{
    for (int i = 0; i < ns->count; i++)
    {
        if (strcmp(ns->keys[i], key) == 0)
        {
            if (ns->vals[i].type == VAL_STR)
                free(ns->vals[i].str);
            ns->vals[i] = val;
            return;
        }
    }
    // Not found — append.
    if (ns->count >= ns->cap)
    {
        ns->cap = ns->cap < GROW_MIN_CAP ? GROW_MIN_CAP : ns->cap * 2;
        ns->keys = (char **)pang_realloc(ns->keys, (size_t)ns->cap * sizeof(char *));
        ns->vals = (Value *)pang_realloc(ns->vals, (size_t)ns->cap * sizeof(Value));
    }
    ns->keys[ns->count] = pang_strdup(key);
    ns->vals[ns->count] = val;
    ns->count++;
}

// =============================================================================
// Value helpers
// =============================================================================

static void value_free(Value v)
{
    if (v.type == VAL_STR)
        free(v.str);
    // VAL_NS pointers are not owned.
}

static Value value_nil(void)
{
    Value v;
    v.type = VAL_NIL;
    return v;
}

static Value value_num(double n)
{
    Value v;
    v.type = VAL_NUM;
    v.num = n;
    return v;
}

static Value value_bool(bool b)
{
    Value v;
    v.type = VAL_BOOL;
    v.boolean = b;
    return v;
}

static Value value_str(const char *s)
{
    Value v;
    v.type = VAL_STR;
    v.str = pang_strdup(s);
    return v;
}

static Value value_ns(Namespace *ns)
{
    Value v;
    v.type = VAL_NS;
    v.ns = ns;
    return v;
}

static Value value_dup(Value v)
{
    if (v.type == VAL_STR)
    {
        return value_str(v.str);
    }
    return v;
}

bool pang_truthy(Value v)
{
    if (v.type == VAL_NIL)
        return false;
    if (v.type == VAL_BOOL)
        return v.boolean;
    return true;
}

// =============================================================================
// Translation table
// =============================================================================

typedef struct
{
    const char *en;
    const char *it;
} Translation;

static const Translation translate_table[] = {
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
static const int translate_count = sizeof(translate_table) / sizeof(translate_table[0]);

const char *pang_tr(PangCtx *ctx, const char *s)
{
    if (ctx->language && strcmp(ctx->language, "italian") == 0)
    {
        for (int i = 0; i < translate_count; i++)
        {
            if (strcmp(translate_table[i].en, s) == 0)
            {
                return translate_table[i].it;
            }
        }
        fprintf(stderr, "can't translate: %s\n", s);
        return s;
    }
    return s;
}

// =============================================================================
// WordDefs helpers (linear search, since there are < 30 entries)
// =============================================================================

static WordDef *worddefs_find(PangCtx *ctx, const char *key)
{
    for (int i = 0; i < ctx->word_defs_count; i++)
    {
        if (strcmp(ctx->word_defs_entries[i].key, key) == 0)
        {
            return &ctx->word_defs_entries[i].def;
        }
    }
    return NULL;
}

static void worddefs_upsert(PangCtx *ctx, const char *key, WordDef def)
{
    for (int i = 0; i < ctx->word_defs_count; i++)
    {
        if (strcmp(ctx->word_defs_entries[i].key, key) == 0)
        {
            ctx->word_defs_entries[i].def = def;
            return;
        }
    }
    // Not found — append.
    if (ctx->word_defs_count >= ctx->word_defs_cap)
    {
        ctx->word_defs_cap = ctx->word_defs_cap < GROW_MIN_CAP ? GROW_MIN_CAP : ctx->word_defs_cap * 2;
        ctx->word_defs_entries = (WordDefsEntry *)pang_realloc(
            ctx->word_defs_entries, (size_t)ctx->word_defs_cap * sizeof(WordDefsEntry));
    }
    ctx->word_defs_entries[ctx->word_defs_count].key = pang_strdup(key);
    ctx->word_defs_entries[ctx->word_defs_count].def = def;
    ctx->word_defs_count++;
}

// =============================================================================
// File path utilities
// =============================================================================

static bool path_is_absolute(const char *path)
{
    if (path[0] == '/')
        return true;
    if (strlen(path) >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
        return true;
    return false;
}

static char *path_dirname(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    if (!last_slash)
        return pang_strdup(".");
    if (last_slash == path)
        return pang_strdup("/");
    size_t len = (size_t)(last_slash - path);
    char *d = (char *)pang_malloc(len + 1);
    memcpy(d, path, len);
    d[len] = '\0';
    return d;
}

static char *resolve_words_filename(PangCtx *ctx, const char *fileName)
{
    if (path_is_absolute(fileName))
        return pang_strdup(fileName);
    if (ctx->file_directory_stack.len == 0)
        return pang_strdup(fileName);
    const char *dir = ctx->file_directory_stack.data[ctx->file_directory_stack.len - 1];
    size_t len = strlen(dir) + 1 + strlen(fileName) + 1;
    char *resolved = (char *)pang_malloc(len);
    snprintf(resolved, len, "%s/%s", dir, fileName);
    return resolved;
}

// =============================================================================
// File reading
// =============================================================================

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0)
    {
        fclose(f);
        return NULL;
    }
    char *buf = (char *)pang_malloc((size_t)sz + 1);
    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[nread] = '\0';
    return buf;
}

// =============================================================================
// Tokenizer / Lexer
// =============================================================================

static char *hashbang_remove(const char *program)
{
    if (program[0] == '#')
    {
        const char *nl = strchr(program, '\n');
        if (nl)
            return pang_strdup(nl + 1);
        return pang_strdup("");
    }
    return pang_strdup(program);
}

static void program_words(PangCtx *ctx, const char *pnProgram)
{
    size_t len = strlen(pnProgram);
    char *token = NULL;
    size_t token_len = 0;
    size_t token_cap = 0;
    char *quoted = NULL;
    size_t quoted_len = 0;
    size_t quoted_cap = 0;
    bool in_string = false;
    bool escaping = false;

#define FLUSH_TOKEN()                                   \
    do                                                  \
    {                                                   \
        if (token_len > 0)                              \
        {                                               \
            token[token_len] = '\0';                    \
            strvec_push(&ctx->words, token);            \
            boolvec_push(&ctx->string_literals, false); \
            token_len = 0;                              \
        }                                               \
    } while (0)

#define FLUSH_QUOTED()                             \
    do                                             \
    {                                              \
        quoted[quoted_len] = '\0';                 \
        strvec_push(&ctx->words, quoted);          \
        boolvec_push(&ctx->string_literals, true); \
        quoted_len = 0;                            \
    } while (0)

#define APPEND_TOKEN(ch)                                     \
    do                                                       \
    {                                                        \
        if (token_len + 1 >= token_cap)                      \
        {                                                    \
            token_cap = token_cap < 64 ? 64 : token_cap * 2; \
            token = (char *)pang_realloc(token, token_cap);  \
        }                                                    \
        token[token_len++] = (ch);                           \
    } while (0)

#define APPEND_QUOTED(ch)                                       \
    do                                                          \
    {                                                           \
        if (quoted_len + 1 >= quoted_cap)                       \
        {                                                       \
            quoted_cap = quoted_cap < 64 ? 64 : quoted_cap * 2; \
            quoted = (char *)pang_realloc(quoted, quoted_cap);  \
        }                                                       \
        quoted[quoted_len++] = (ch);                            \
    } while (0)

    for (size_t i = 0; i < len; i++)
    {
        char ch = pnProgram[i];
        if (in_string)
        {
            if (escaping)
            {
                if (ch == '"')
                    APPEND_QUOTED('"');
                else if (ch == '\\')
                    APPEND_QUOTED('\\');
                else if (ch == 'n')
                    APPEND_QUOTED('\n');
                else if (ch == 't')
                    APPEND_QUOTED('\t');
                else
                    fprintf(stderr, "invalid escape sequence: \\%c\n", ch);
                escaping = false;
            }
            else if (ch == '\\')
            {
                escaping = true;
            }
            else if (ch == '"')
            {
                FLUSH_QUOTED();
                in_string = false;
            }
            else
            {
                APPEND_QUOTED(ch);
            }
        }
        else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
        {
            FLUSH_TOKEN();
        }
        else if (ch == '"')
        {
            FLUSH_TOKEN();
            in_string = true;
        }
        else
        {
            APPEND_TOKEN(ch);
        }
    }

    if (escaping)
        fprintf(stderr, "unterminated escape sequence in string literal\n");
    if (in_string)
        fprintf(stderr, "unterminated string literal\n");

    FLUSH_TOKEN();

    free(token);
    free(quoted);

#undef APPEND_TOKEN
#undef APPEND_QUOTED
#undef FLUSH_TOKEN
#undef FLUSH_QUOTED
}

// =============================================================================
// Core Evaluator
// =============================================================================

static int phrase_length(PangCtx *ctx, int wordIndex);

static Value evaluate_word(PangCtx *ctx, int wordIndex);

static int phrase_length(PangCtx *ctx, int wordIndex)
{
    int idx = wordIndex - 1;
    if (idx >= ctx->words.len)
        return 1;
    const char *word = ctx->words.data[idx];
    int length = 1;

    // String literals always have length 1.
    if (idx < ctx->string_literals.len && ctx->string_literals.data[idx])
        return 1;

    // do ... end blocks
    if (strcmp(word, pang_tr(ctx, "do")) == 0)
    {
        while (1)
        {
            int lookAhead = wordIndex + length - 1;
            if (lookAhead >= ctx->words.len ||
                strcmp(ctx->words.data[lookAhead], pang_tr(ctx, "end")) == 0)
                return length + 1;
            length += phrase_length(ctx, wordIndex + length);
        }
    }

    // Numbers have length 1.
    {
        char *end = NULL;
        strtod(word, &end);
        if (end != word && *end == '\0')
            return 1;
    }

    WordDef *def = worddefs_find(ctx, word);
    if (!def)
        return 1;

    for (int i = 0; i < def->arity; i++)
        length += phrase_length(ctx, wordIndex + length);
    return length;
}

static Value evaluate_word(PangCtx *ctx, int wordIndex)
{
    int idx = wordIndex - 1;
    if (idx >= ctx->words.len)
        return value_nil();
    const char *word = ctx->words.data[idx];

    // String literals return their raw value.
    if (idx < ctx->string_literals.len && ctx->string_literals.data[idx])
        return value_str(word);

    // Number literals.
    {
        char *end = NULL;
        double val = strtod(word, &end);
        if (end != word && *end == '\0')
            return value_num(val);
    }

    // do ... end blocks
    if (strcmp(word, pang_tr(ctx, "do")) == 0)
    {
        int doWordIndex = wordIndex + 1;
        Value evaluated = value_nil();
        while (doWordIndex - 1 < ctx->words.len &&
               strcmp(ctx->words.data[doWordIndex - 1], pang_tr(ctx, "end")) != 0)
        {
            Value prev = evaluated;
            evaluated = evaluate_word(ctx, doWordIndex);
            value_free(prev);
            doWordIndex += phrase_length(ctx, doWordIndex);
        }
        return evaluated;
    }

    WordDef *def = worddefs_find(ctx, word);
    if (!def)
    {
        printf("%s\"%s\"%s\n", pang_tr(ctx, "word:"), word, pang_tr(ctx, " definition not found"));
        return value_nil();
    }

    // Build argument indices array
    int *args = (int *)pang_malloc((size_t)PANG_MAX(def->arity, 1) * sizeof(int));
    int argWordIndex = wordIndex + 1;
    for (int i = 0; i < def->arity; i++)
    {
        args[i] = argWordIndex;
        argWordIndex += phrase_length(ctx, argWordIndex);
    }

    Value result;
    if (def->is_closure)
    {
        // Closure: create a new frame from arg indices, call body
        Namespace *frame = namespace_new();
        for (int i = 0; i < def->arity; i++)
        {
            char idx_str[16];
            snprintf(idx_str, sizeof(idx_str), "%d", i + 1);
            Value argVal = evaluate_word(ctx, args[i]);
            namespace_set(frame, idx_str, argVal);
            // argVal.val_owned strings are now owned by frame
            // but we also need to handle freeing the argVal's str if it was a temp
            // evaluate_word returns a value; for strings, it's a strdup.
            // When we set into namespace, we don't want double ownership.
            // We'll handle this by NOT freeing argVal — namespace_set takes ownership.
        }
        nsvec_push(&ctx->call_stack, frame);
        result = evaluate_word(ctx, def->closure_body_index);
        // Pop the frame (namespace_free will free the frame and its owned values)
        Namespace *popped = nsvec_pop(&ctx->call_stack);
        namespace_free(popped);
    }
    else
    {
        result = def->fn(ctx, args);
    }

    free(args);
    return result;
}

// =============================================================================
// Program Execution
// =============================================================================

static void execute_program(PangCtx *ctx, const char *pnProgram)
{
    const char *do_tr = pang_tr(ctx, "do");
    const char *end_tr = pang_tr(ctx, "end");
    size_t wrap_len = strlen(do_tr) + 1 + strlen(pnProgram) + 1 + strlen(end_tr) + 1;
    char *wrapped = (char *)pang_malloc(wrap_len);
    snprintf(wrapped, wrap_len, "%s %s %s", do_tr, pnProgram, end_tr);

    int words_before = ctx->words.len;
    program_words(ctx, wrapped);
    free(wrapped);

    if (ctx->words.len == words_before)
        return;

    Value result = evaluate_word(ctx, words_before + 1);
    value_free(result);
}

static void execute_words_file(PangCtx *ctx, const char *fileName)
{
    char *resolved = resolve_words_filename(ctx, fileName);
    char *content = read_file(resolved);

    // Fallback to original name
    if (!content && strcmp(resolved, fileName) != 0)
    {
        free(resolved);
        resolved = pang_strdup(fileName);
        content = read_file(resolved);
    }

    if (!content)
    {
        fprintf(stderr, "cannot open words file: %s\n", fileName);
        free(resolved);
        return;
    }

    char *program = hashbang_remove(content);
    free(content);

    char *dirname = path_dirname(resolved);
    strvec_push(&ctx->file_directory_stack, dirname);
    free(dirname);

    execute_program(ctx, program);
    free(program);

    // Pop directory stack
    if (ctx->file_directory_stack.len > 0)
    {
        free(ctx->file_directory_stack.data[ctx->file_directory_stack.len - 1]);
        ctx->file_directory_stack.len--;
    }
    free(resolved);
}

static void read_execute_loop(PangCtx *ctx)
{
    volatile bool reopened_tty = false;
    char line_buf[4096];
    while (!ctx->exit_requested)
    {
        printf("> ");
        fflush(stdout);
        if (!fgets(line_buf, sizeof(line_buf), stdin))
        {
            // EOF — try to reopen /dev/tty once
            if (!reopened_tty)
            {
                reopened_tty = true;
                if (freopen("/dev/tty", "r", stdin) != NULL)
                    continue;
            }
            break;
        }

        reopened_tty = false;

        // Strip trailing newline
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r'))
            line_buf[--len] = '\0';

        if (len == 0)
            continue;

        // Check for direct exit command
        if (strcmp(line_buf, "exit") == 0 || strcmp(line_buf, "esci") == 0)
            break;

        if (setjmp(ctx->exit_jmpbuf) == 0)
        {
            execute_program(ctx, line_buf);
        }
        else
        {
            // longjmp from exit word
            if (ctx->exit_requested)
                break;
        }
    }
}

void pang_run(PangCtx *ctx, int argc, char *argv[])
{
    volatile const char *filename = NULL;
    volatile bool repl = false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "italian") == 0)
        {
            // Already handled
        }
        else if (strcmp(argv[i], "-") == 0 || strcmp(argv[i], "--repl") == 0)
        {
            repl = true;
        }
        else
        {
            filename = argv[i];
        }
    }

    if (filename)
    {
        volatile const char *fname = filename;
        if (setjmp(ctx->exit_jmpbuf) == 0)
        {
            execute_words_file(ctx, fname);
        }
        // exit was requested — cleanly return
    }

    if (!ctx->exit_requested && (repl || !filename))
    {
        read_execute_loop(ctx);
    }
}

// =============================================================================
// Built-in Word Definitions
// =============================================================================

// Word function signatures:  Value fn(PangCtx *ctx, const int *args)

static Value word_print(PangCtx *ctx, const int *args)
{
    Value val = evaluate_word(ctx, args[0]);
    switch (val.type)
    {
    case VAL_NIL:
        printf("nil\n");
        break;
    case VAL_NUM:
        printf("%g\n", val.num);
        break;
    case VAL_BOOL:
        printf("%s\n", val.boolean ? "true" : "false");
        break;
    case VAL_STR:
        printf("%s\n", val.str);
        break;
    case VAL_NS:
        printf("[namespace]\n");
        break;
    }
    return val; // caller may value_free this
}

static Value word_read_text(PangCtx *ctx, const int *args)
{
    (void)ctx;
    (void)args;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin))
        return value_str("");
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return value_str(buf);
}

static Value word_to_number(PangCtx *ctx, const int *args)
{
    Value val = evaluate_word(ctx, args[0]);
    if (val.type == VAL_STR)
    {
        char *end = NULL;
        double n = strtod(val.str, &end);
        bool valid = (end != val.str && *end == '\0');
        value_free(val);
        if (valid)
            return value_num(n);
        return value_num(0.0);
    }
    return val;
}

static Value word_add(PangCtx *ctx, const int *args)
{
    Value a = evaluate_word(ctx, args[0]);
    Value b = evaluate_word(ctx, args[1]);
    double result = a.num + b.num;
    value_free(a);
    value_free(b);
    return value_num(result);
}

static Value word_multiply(PangCtx *ctx, const int *args)
{
    Value a = evaluate_word(ctx, args[0]);
    Value b = evaluate_word(ctx, args[1]);
    double result = a.num * b.num;
    value_free(a);
    value_free(b);
    return value_num(result);
}

static Value word_true(PangCtx *ctx, const int *args)
{
    (void)ctx;
    (void)args;
    return value_bool(true);
}

static Value word_false(PangCtx *ctx, const int *args)
{
    (void)ctx;
    (void)args;
    return value_bool(false);
}

static Value word_if(PangCtx *ctx, const int *args)
{
    Value cond = evaluate_word(ctx, args[0]);
    Value result;
    if (pang_truthy(cond))
    {
        value_free(cond);
        result = evaluate_word(ctx, args[1]);
    }
    else
    {
        value_free(cond);
        result = evaluate_word(ctx, args[2]);
    }
    return result;
}

static Value word_while(PangCtx *ctx, const int *args)
{
    for (;;)
    {
        Value cond = evaluate_word(ctx, args[0]);
        bool keep = pang_truthy(cond);
        value_free(cond);
        if (!keep)
            break;
        Value result = evaluate_word(ctx, args[1]);
        if (result.type == VAL_STR && strcmp(result.str, "break") == 0)
        {
            value_free(result);
            break;
        }
        value_free(result);
    }
    return value_nil();
}

static Value word_repeat(PangCtx *ctx, const int *args)
{
    Value total = evaluate_word(ctx, args[0]);
    if (total.type != VAL_NUM)
    {
        value_free(total);
        return value_nil();
    }
    int64_t n = (int64_t)total.num;
    value_free(total);
    Value result = value_nil();
    for (int64_t i = 0; i < n; i++)
    {
        Value prev = result;
        result = evaluate_word(ctx, args[1]);
        value_free(prev);
    }
    return result;
}

static Value word_not(PangCtx *ctx, const int *args)
{
    Value v = evaluate_word(ctx, args[0]);
    bool b = !pang_truthy(v);
    value_free(v);
    return value_bool(b);
}

static Value word_and(PangCtx *ctx, const int *args)
{
    Value a = evaluate_word(ctx, args[0]);
    bool ta = pang_truthy(a);
    value_free(a);
    if (!ta)
        return value_bool(false);
    Value b = evaluate_word(ctx, args[1]);
    bool tb = pang_truthy(b);
    value_free(b);
    return value_bool(tb);
}

static Value word_or(PangCtx *ctx, const int *args)
{
    Value a = evaluate_word(ctx, args[0]);
    bool ta = pang_truthy(a);
    value_free(a);
    if (ta)
        return value_bool(true);
    Value b = evaluate_word(ctx, args[1]);
    bool tb = pang_truthy(b);
    value_free(b);
    return value_bool(tb);
}

static Value word_equal(PangCtx *ctx, const int *args)
{
    Value a = evaluate_word(ctx, args[0]);
    Value b = evaluate_word(ctx, args[1]);

    if (a.type != b.type)
    {
        value_free(a);
        value_free(b);
        return value_bool(false);
    }

    bool eq = false;
    switch (a.type)
    {
    case VAL_NIL:
        eq = true;
        break;
    case VAL_NUM:
        eq = (a.num == b.num);
        break;
    case VAL_BOOL:
        eq = (a.boolean == b.boolean);
        break;
    case VAL_STR:
        eq = (strcmp(a.str, b.str) == 0);
        break;
    case VAL_NS:
        eq = (a.ns == b.ns);
        break;
    }
    value_free(a);
    value_free(b);
    return value_bool(eq);
}

static Value word_set(PangCtx *ctx, const int *args)
{
    Namespace *vars = ctx->call_stack.data[ctx->call_stack.len - 1];
    Value nameVal = evaluate_word(ctx, args[0]);
    Value val = evaluate_word(ctx, args[1]);
    if (nameVal.type == VAL_STR)
    {
        namespace_set(vars, nameVal.str, val);
        value_free(nameVal);
        // Don't free val — namespace_set takes ownership
    }
    else
    {
        value_free(nameVal);
        value_free(val);
    }
    return value_nil();
}

static Value word_get(PangCtx *ctx, const int *args)
{
    Namespace *vars = ctx->call_stack.data[ctx->call_stack.len - 1];
    Value nameVal = evaluate_word(ctx, args[0]);
    if (nameVal.type != VAL_STR)
    {
        value_free(nameVal);
        return value_nil();
    }
    Value out;
    if (!namespace_get(vars, nameVal.str, &out))
    {
        printf("nil returning from get_function()\n");
        value_free(nameVal);
        return value_nil();
    }
    value_free(nameVal);
    return value_dup(out);
}

static Value word_variable_set(PangCtx *ctx, const int *args)
{
    Value nsVal = evaluate_word(ctx, args[0]);
    Value nameVal = evaluate_word(ctx, args[1]);
    Value val = evaluate_word(ctx, args[2]);
    if (nsVal.type == VAL_NS && nameVal.type == VAL_STR)
    {
        namespace_set(nsVal.ns, nameVal.str, val);
    }
    else
    {
        value_free(val);
    }
    // nsVal.ns is borrowed — don't free the pointer
    value_free(nsVal);
    value_free(nameVal);
    return value_nil();
}

static Value word_variable_get(PangCtx *ctx, const int *args)
{
    Value nsVal = evaluate_word(ctx, args[0]);
    Value nameVal = evaluate_word(ctx, args[1]);
    Value result = value_nil();
    if (nsVal.type == VAL_NS && nameVal.type == VAL_STR)
    {
        Value out;
        if (namespace_get(nsVal.ns, nameVal.str, &out))
            result = value_dup(out);
    }
    value_free(nsVal);
    value_free(nameVal);
    return result;
}

static Value word_namespace_fn(PangCtx *ctx, const int *args)
{
    (void)args;
    return value_ns(ctx->call_stack.data[ctx->call_stack.len - 1]);
}

static Value word_modulus(PangCtx *ctx, const int *args)
{
    Value aVal = evaluate_word(ctx, args[0]);
    Value bVal = evaluate_word(ctx, args[1]);
    int64_t a = (int64_t)aVal.num;
    int64_t b = (int64_t)bVal.num;
    value_free(aVal);
    value_free(bVal);
    return value_num((double)(a % b));
}

static Value word_greater(PangCtx *ctx, const int *args)
{
    Value aVal = evaluate_word(ctx, args[0]);
    Value bVal = evaluate_word(ctx, args[1]);
    bool gt = aVal.num > bVal.num;
    value_free(aVal);
    value_free(bVal);
    return value_bool(gt);
}

static Value word_help(PangCtx *ctx, const int *args)
{
    (void)args;
    for (int i = 0; i < ctx->word_defs_count; i++)
    {
        printf("%s<%d ", ctx->word_defs_entries[i].key, ctx->word_defs_entries[i].def.arity);
    }
    printf("\n");
    return value_nil();
}

static Value word_execute_file(PangCtx *ctx, const int *args)
{
    const char *fileName = ctx->words.data[args[0] - 1];
    execute_words_file(ctx, fileName);
    return value_nil();
}

static Value word_dont(PangCtx *ctx, const int *args)
{
    (void)ctx;
    (void)args;
    return value_nil();
}

static Value word_define_word(PangCtx *ctx, const int *args)
{
    Value nameVal = evaluate_word(ctx, args[0]);
    Value arityVal = evaluate_word(ctx, args[1]);
    int bodyIndex = args[2];

    if (nameVal.type != VAL_STR || arityVal.type != VAL_NUM)
    {
        value_free(nameVal);
        value_free(arityVal);
        return value_nil();
    }

    int arity = (int)arityVal.num;
    value_free(arityVal);

    WordDef wd;
    wd.arity = arity;
    wd.fn = NULL;
    wd.closure_body_index = bodyIndex;
    wd.is_closure = true;
    worddefs_upsert(ctx, nameVal.str, wd);
    value_free(nameVal);
    return value_nil();
}

static Value word_argument(PangCtx *ctx, const int *args)
{
    Namespace *frame = ctx->call_stack.data[ctx->call_stack.len - 1];
    Value idxVal = evaluate_word(ctx, args[0]);
    if (idxVal.type != VAL_NUM)
    {
        value_free(idxVal);
        return value_nil();
    }
    char idx_str[16];
    snprintf(idx_str, sizeof(idx_str), "%d", (int)idxVal.num);
    value_free(idxVal);
    Value out;
    if (namespace_get(frame, idx_str, &out))
        return value_dup(out);
    return value_nil();
}

static Value word_command_prompt(PangCtx *ctx, const int *args)
{
    (void)args;
    read_execute_loop(ctx);
    return value_nil();
}

static Value word_exit(PangCtx *ctx, const int *args)
{
    (void)args;
    ctx->exit_requested = true;
    longjmp(ctx->exit_jmpbuf, 1);
    return value_nil(); // unreachable
}

static Value word_increment(PangCtx *ctx, const int *args)
{
    Namespace *vars = ctx->call_stack.data[ctx->call_stack.len - 1];
    Value nameVal = evaluate_word(ctx, args[0]);
    if (nameVal.type != VAL_STR)
    {
        value_free(nameVal);
        return value_nil();
    }
    Value existing;
    double val = 0.0;
    if (namespace_get(vars, nameVal.str, &existing))
    {
        if (existing.type == VAL_NUM)
            val = existing.num;
        // Note: we don't free existing here because it's just a copy from namespace_get
    }
    val += 1.0;
    namespace_set(vars, nameVal.str, value_num(val));
    value_free(nameVal);
    return value_num(val);
}

// =============================================================================
// Initialization
// =============================================================================

static void register_builtin(PangCtx *ctx, const char *key, int arity, WordFn fn)
{
    WordDef wd;
    wd.arity = arity;
    wd.fn = fn;
    wd.closure_body_index = 0;
    wd.is_closure = false;
    worddefs_upsert(ctx, key, wd);
}

void pang_init(PangCtx *ctx)
{
    char *saved_lang = ctx->language;
    memset(ctx, 0, sizeof(*ctx));
    ctx->language = saved_lang;

    // Initialize call stack with one empty frame.
    nsvec_push(&ctx->call_stack, namespace_new());

    // Register all built-in words.
    register_builtin(ctx, pang_tr(ctx, "print"), 1, word_print);
    register_builtin(ctx, pang_tr(ctx, "read_text"), 0, word_read_text);
    register_builtin(ctx, pang_tr(ctx, "to_number"), 1, word_to_number);
    register_builtin(ctx, pang_tr(ctx, "add"), 2, word_add);
    register_builtin(ctx, pang_tr(ctx, "multiply"), 2, word_multiply);
    register_builtin(ctx, pang_tr(ctx, "true"), 0, word_true);
    register_builtin(ctx, pang_tr(ctx, "false"), 0, word_false);
    register_builtin(ctx, pang_tr(ctx, "if"), 3, word_if);
    register_builtin(ctx, pang_tr(ctx, "while"), 2, word_while);
    register_builtin(ctx, pang_tr(ctx, "repeat"), 2, word_repeat);
    register_builtin(ctx, pang_tr(ctx, "not"), 1, word_not);
    register_builtin(ctx, pang_tr(ctx, "and"), 2, word_and);
    register_builtin(ctx, pang_tr(ctx, "or"), 2, word_or);
    register_builtin(ctx, pang_tr(ctx, "equal"), 2, word_equal);
    register_builtin(ctx, pang_tr(ctx, "set"), 2, word_set);
    register_builtin(ctx, pang_tr(ctx, "get"), 1, word_get);
    register_builtin(ctx, pang_tr(ctx, "variable_set"), 3, word_variable_set);
    register_builtin(ctx, pang_tr(ctx, "variable_get"), 2, word_variable_get);
    register_builtin(ctx, "namespace", 0, word_namespace_fn);
    register_builtin(ctx, pang_tr(ctx, "modulus"), 2, word_modulus);
    register_builtin(ctx, pang_tr(ctx, "greater"), 2, word_greater);
    register_builtin(ctx, "?", 0, word_help);
    register_builtin(ctx, "!", 1, word_execute_file);
    register_builtin(ctx, pang_tr(ctx, "dont"), 1, word_dont);
    register_builtin(ctx, pang_tr(ctx, "define_word"), 3, word_define_word);
    register_builtin(ctx, pang_tr(ctx, "argument"), 1, word_argument);
    register_builtin(ctx, pang_tr(ctx, "command_prompt"), 0, word_command_prompt);
    register_builtin(ctx, pang_tr(ctx, "exit"), 0, word_exit);
    register_builtin(ctx, pang_tr(ctx, "increment"), 1, word_increment);
}

void pang_set_language(PangCtx *ctx, const char *lang)
{
    free(ctx->language);
    ctx->language = lang ? pang_strdup(lang) : NULL;
}

void pang_destroy(PangCtx *ctx)
{
    // Free word definitions entries
    for (int i = 0; i < ctx->word_defs_count; i++)
        free(ctx->word_defs_entries[i].key);
    free(ctx->word_defs_entries);

    // Free tokenized words
    strvec_free(&ctx->words);
    free(ctx->string_literals.data);

    // Free call stack frames (all owned Namespaces)
    for (int i = 0; i < ctx->call_stack.len; i++)
        namespace_free(ctx->call_stack.data[i]);
    free(ctx->call_stack.data);

    // Free file directory stack
    strvec_free(&ctx->file_directory_stack);

    free(ctx->language);
}