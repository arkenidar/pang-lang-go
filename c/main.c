// Pang: Polish notation language interpreter (v028) — entry point (C port)
//
// Usage: pang [italian] [filename] [--repl|-]
//   filenames ending in .words or .parole are word-definition files
//   "-" or "--repl" starts REPL mode

#include "pang.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // Parse language argument first.
    const char *lang = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "italian") == 0)
        {
            lang = "italian";
            break;
        }
    }

    // Zero-initialize, set language, then init (so word defs get translated keys).
    PangCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    if (lang)
        ctx.language = strdup(lang); // pang_init will memset, save here
    pang_init(&ctx);

    printf("%s028\n", pang_tr(&ctx, "pang version: "));
    printf("? for help\n");

    pang_run(&ctx, argc, argv);

    printf("bye\n");
    pang_destroy(&ctx);
    return 0;
}
