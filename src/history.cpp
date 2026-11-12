#include "history.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <pwd.h>
#include <unistd.h>

#include "builtins.h"

static const size_t kMaxStored = 20;   // spec: remember 20 commands
static const size_t kDefaultShown = 10;  // spec: "history" prints 10

static std::vector<std::string> g_history;   // oldest first

// History lives in the user's real home directory, so it survives across
// sessions no matter which folder the shell is started from.
static std::string historyFile() {
    const char* home = getenv("HOME");
    if (!home || !*home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : ".";
    }
    return std::string(home) + "/.posix_shell_history";
}

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

void loadHistory() {
    std::ifstream in(historyFile());
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (!line.empty()) g_history.push_back(line);
    }
    // If the file somehow holds more, keep only the newest 20.
    if (g_history.size() > kMaxStored)
        g_history.erase(g_history.begin(), g_history.end() - kMaxStored);
}

// Write to a temporary file, then rename it over the real one.
// rename() is atomic, so a crash mid-write can never leave a half-written file.
static void saveHistory() {
    std::string path = historyFile();
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return;   // e.g. read-only home: history still works in memory
        for (const std::string& h : g_history) out << h << '\n';
    }
    if (rename(tmp.c_str(), path.c_str()) < 0) perror("history: rename");
}

void addHistory(const std::string& raw) {
    std::string line = trim(raw);
    if (line.empty()) return;
    if (!g_history.empty() && g_history.back() == line) return;   // "cd" twice is stored once

    g_history.push_back(line);
    if (g_history.size() > kMaxStored) g_history.erase(g_history.begin());   // drop the oldest
    saveHistory();
}

const std::vector<std::string>& historyEntries() { return g_history; }

// history [N]   prints the latest N commands (default 10), oldest first
int builtin_history(const std::vector<std::string>& argv) {
    if (argv.size() > 2) {
        fprintf(stderr, "history: Invalid arguments\n");
        return 1;
    }
    size_t n = kDefaultShown;
    if (argv.size() == 2) {
        const std::string& a = argv[1];
        if (a.empty() || a.find_first_not_of("0123456789") != std::string::npos) {
            fprintf(stderr, "history: %s: numeric argument required\n", a.c_str());
            return 1;
        }
        n = std::stoul(a.substr(0, 6));   // substr: avoid overflow on absurd input
    }
    n = std::min(n, g_history.size());
    for (size_t i = g_history.size() - n; i < g_history.size(); ++i)
        printf("%s\n", g_history[i].c_str());
    return 0;
}
