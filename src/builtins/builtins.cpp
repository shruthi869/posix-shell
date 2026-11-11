#include "builtins.h"

#include <unordered_map>

static bool g_exitRequested = false;

int builtin_exit(const std::vector<std::string>&) {
    g_exitRequested = true;
    return 0;
}

bool shellShouldExit() { return g_exitRequested; }

// Name -> function table. Adding a built-in = write the function + add one line here.
BuiltinFn findBuiltin(const std::string& name) {
    static const std::unordered_map<std::string, BuiltinFn> table = {
        {"cd", builtin_cd},
        {"pwd", builtin_pwd},
        {"echo", builtin_echo},
        {"exit", builtin_exit},
        {"ls", builtin_ls},
        {"pinfo", builtin_pinfo},
        {"search", builtin_search},
        {"jobs", builtin_jobs},
        {"fg", builtin_fg},
        {"bg", builtin_bg},
    };
    auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}
