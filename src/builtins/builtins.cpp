#include "builtins.h"

#include <algorithm>
#include <unordered_map>

static bool g_exitRequested = false;

int builtin_exit(const std::vector<std::string>&) {
    g_exitRequested = true;
    return 0;
}

bool shellShouldExit() { return g_exitRequested; }

// Name -> function table. Adding a built-in = write the function + add one line here.
static const std::unordered_map<std::string, BuiltinFn>& table() {
    static const std::unordered_map<std::string, BuiltinFn> t = {
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
        {"history", builtin_history},
    };
    return t;
}

BuiltinFn findBuiltin(const std::string& name) {
    auto it = table().find(name);
    return it == table().end() ? nullptr : it->second;
}

const std::vector<std::string>& builtinNames() {
    static std::vector<std::string> names;
    if (names.empty()) {
        for (const auto& kv : table()) names.push_back(kv.first);
        std::sort(names.begin(), names.end());
    }
    return names;
}
