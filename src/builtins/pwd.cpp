#include <cstdio>

#include "builtins.h"
#include "prompt.h"   // currentDir()

// Prints the absolute path of the current directory (never with "~").
int builtin_pwd(const std::vector<std::string>&) {
    std::string dir = currentDir();
    if (dir == "?") return 1;   // getcwd failed; currentDir already printed why
    printf("%s\n", dir.c_str());
    return 0;
}
