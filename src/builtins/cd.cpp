#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>   // chdir

#include "builtins.h"
#include "prompt.h"   // shellHome(), currentDir()

// Previous directory, for "cd -". The spec forbids the OLDPWD variable,
// so the shell remembers it itself.
static std::string g_prevDir;

int builtin_cd(const std::vector<std::string>& argv) {
    if (argv.size() > 2) {
        fprintf(stderr, "cd: Invalid arguments\n");
        return 1;
    }

    // No argument means "go home" (the shell's home, i.e. where it was started).
    std::string arg = (argv.size() == 2) ? argv[1] : "~";
    std::string target;
    bool printNewDir = false;

    if (arg == "-") {
        if (g_prevDir.empty()) {
            fprintf(stderr, "cd: no previous directory\n");
            return 1;
        }
        target = g_prevDir;
        printNewDir = true;   // like bash, "cd -" prints where it went
    } else if (arg == "~") {
        target = shellHome();
    } else if (arg.rfind("~/", 0) == 0) {          // "~/sub/dir"
        target = shellHome() + arg.substr(1);
    } else {
        target = arg;   // ".", "..", relative or absolute: chdir understands these
    }

    std::string before = currentDir();
    if (chdir(target.c_str()) < 0) {
        fprintf(stderr, "cd: %s: %s\n", arg.c_str(), strerror(errno));
        return 1;
    }
    g_prevDir = before;   // only update after a successful change

    if (printNewDir) printf("%s\n", currentDir().c_str());
    return 0;
}
