#include <cstdio>

#include "builtins.h"

// Prints the arguments separated by single spaces.
// Extra spaces/tabs between words were already removed by the parser, and
// quotes are printed as-is (the spec does not require quote handling).
int builtin_echo(const std::vector<std::string>& argv) {
    for (size_t i = 1; i < argv.size(); ++i) {
        if (i > 1) putchar(' ');
        fputs(argv[i].c_str(), stdout);
    }
    putchar('\n');
    return 0;
}
