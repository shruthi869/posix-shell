#include <cstdio>
#include <dirent.h>     // opendir, readdir, closedir
#include <sys/stat.h>   // lstat

#include "builtins.h"

// Depth-first search for an entry called `target` anywhere under `dir`.
// Stops as soon as it is found.
static bool searchIn(const std::string& dir, const std::string& target) {
    DIR* d = opendir(dir.c_str());
    if (!d) return false;   // unreadable folder (e.g. permission denied): skip it

    bool found = false;
    while (struct dirent* de = readdir(d)) {
        std::string name = de->d_name;
        if (name == "." || name == "..") continue;   // would loop forever

        if (name == target) {
            found = true;
            break;
        }

        std::string path = dir + "/" + name;
        struct stat st;
        // lstat: do NOT follow symlinks, so a link pointing back up the tree
        // cannot send us round in circles.
        if (lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && searchIn(path, target)) {
            found = true;
            break;
        }
    }
    closedir(d);   // always close, including on early exit
    return found;
}

// search <name>   prints True or False
int builtin_search(const std::vector<std::string>& argv) {
    if (argv.size() != 2) {
        fprintf(stderr, "search: Invalid arguments\n");
        return 1;
    }
    bool found = searchIn(".", argv[1]);
    printf(found ? "True\n" : "False\n");
    return found ? 0 : 1;
}
