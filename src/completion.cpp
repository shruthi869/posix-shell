#include "completion.h"

#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <set>
#include <sstream>
#include <sys/stat.h>

#include "builtins.h"

// All command names: built-ins + every file in each $PATH folder.
// Scanning PATH takes a moment, so do it once and reuse the result.
//
// Speed: readdir already reports each entry's type (d_type), so we don't
// stat() every file to check its execute bit. On WSL, PATH includes Windows
// folders under /mnt/c with thousands of files, where one stat() per file
// took ~9 s; listing alone takes well under a second.
static const std::vector<std::string>& commandNames() {
    static std::vector<std::string> names;
    static bool loaded = false;
    if (loaded) return names;
    loaded = true;

    std::set<std::string> unique(builtinNames().begin(), builtinNames().end());
    const char* path = getenv("PATH");
    std::stringstream dirs(path ? path : "");
    std::string dir;
    while (std::getline(dirs, dir, ':')) {          // PATH = "/usr/bin:/bin:..."
        if (dir.empty()) continue;
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        while (struct dirent* de = readdir(d)) {
            if (de->d_name[0] == '.') continue;
            bool isFile = de->d_type == DT_REG || de->d_type == DT_LNK;
            if (de->d_type == DT_UNKNOWN) {         // filesystem didn't say: ask stat
                struct stat st;
                std::string full = dir + "/" + de->d_name;
                isFile = stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode);
            }
            if (isFile) unique.insert(de->d_name);
        }
        closedir(d);
    }
    names.assign(unique.begin(), unique.end());     // std::set is already sorted
    return names;
}

void findCompletions(const std::string& line, std::string& prefix, std::vector<Candidate>& matches) {
    matches.clear();

    // The word being typed = everything after the last space/tab.
    size_t start = line.find_last_of(" \t|;&<>") ;
    start = (start == std::string::npos) ? 0 : start + 1;
    std::string word = line.substr(start);

    // Is it in "command position"? Only spaces (or nothing) before it since
    // the start of the line or since the last | ; &
    std::string before = line.substr(0, start);
    size_t last = before.find_last_not_of(" \t");
    bool isCommand = (last == std::string::npos) || before[last] == '|' || before[last] == ';' ||
                     before[last] == '&';

    if (isCommand && word.find('/') == std::string::npos) {
        prefix = word;
        for (const std::string& c : commandNames())
            if (c.compare(0, prefix.size(), prefix) == 0) matches.push_back({c, false});
        return;
    }

    // File completion. "src/pa" -> look in folder "src" for names starting with "pa".
    size_t slash = word.rfind('/');
    std::string dir = (slash == std::string::npos) ? "." : word.substr(0, slash + 1);
    prefix = (slash == std::string::npos) ? word : word.substr(slash + 1);

    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (struct dirent* de = readdir(d)) {
        std::string name = de->d_name;
        if (name == "." || name == "..") continue;
        if (name[0] == '.' && (prefix.empty() || prefix[0] != '.')) continue;   // hidden unless asked
        if (name.compare(0, prefix.size(), prefix) != 0) continue;
        struct stat st;
        bool isDir = stat((dir + "/" + name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        matches.push_back({name, isDir});
    }
    closedir(d);
    std::sort(matches.begin(), matches.end(),
              [](const Candidate& a, const Candidate& b) { return a.name < b.name; });
}
