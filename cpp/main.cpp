// Pang: Polish notation language interpreter (v028) — entry point
//
// Usage: pang [italian] [filename] [-]
//   filenames ending in .words or .parole are word-definition files
//   "-" starts REPL mode

#include "interpreter.hpp"

#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    pang::PangInterpreter interpreter;

    // Parse language argument before initializing word defs,
    // so that Italian word definitions are registered correctly.
    std::string lang;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string{argv[i]} == "italian")
        {
            lang = "italian";
            break;
        }
    }
    interpreter.setLanguage(lang);
    interpreter.initWordDefs();

    std::cout << interpreter.tr("pang version: ") << "028\n";
    std::cout << "? for help\n";

    interpreter.run(argc, argv);

    std::cout << "bye\n";
    return 0;
}
