#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>     // opendir, readdir, closedir
#include <grp.h>        // getgrgid
#include <pwd.h>        // getpwuid
#include <sys/stat.h>   // stat, lstat, S_IS*
#include <unistd.h>     // readlink

#include "builtins.h"
#include "prompt.h"     // shellHome()

namespace {

struct Entry {
    std::string name;    // what we print
    std::string path;    // what we stat
    struct stat st;
};

bool byName(const Entry& a, const Entry& b) { return a.name < b.name; }

// "drwxr-xr-x": file type + 3 permission triples (owner, group, others)
std::string modeString(mode_t m) {
    std::string s(10, '-');
    if (S_ISDIR(m)) s[0] = 'd';
    else if (S_ISLNK(m)) s[0] = 'l';
    else if (S_ISCHR(m)) s[0] = 'c';
    else if (S_ISBLK(m)) s[0] = 'b';
    else if (S_ISFIFO(m)) s[0] = 'p';
    else if (S_ISSOCK(m)) s[0] = 's';

    const char rwx[] = "rwxrwxrwx";
    const mode_t bits[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP,
                           S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    for (int i = 0; i < 9; ++i)
        if (m & bits[i]) s[i + 1] = rwx[i];

    // Special bits replace the execute letter: s/S (setuid, setgid), t/T (sticky)
    if (m & S_ISUID) s[3] = (m & S_IXUSR) ? 's' : 'S';
    if (m & S_ISGID) s[6] = (m & S_IXGRP) ? 's' : 'S';
    if (m & S_ISVTX) s[9] = (m & S_IXOTH) ? 't' : 'T';
    return s;
}

std::string ownerName(uid_t uid) {
    struct passwd* pw = getpwuid(uid);
    return pw ? pw->pw_name : std::to_string(uid);
}

std::string groupName(gid_t gid) {
    struct group* gr = getgrgid(gid);
    return gr ? gr->gr_name : std::to_string(gid);
}

// Recent files (last 6 months): "Nov 11 07:18". Older or future: "Nov 11  2025".
std::string timeString(time_t t) {
    char buf[64];
    time_t now = time(nullptr);
    const time_t sixMonths = 31556952 / 2;   // GNU ls uses half an average year
    bool recent = (t <= now) && (now - t < sixMonths);
    strftime(buf, sizeof buf, recent ? "%b %e %H:%M" : "%b %e  %Y", localtime(&t));
    return buf;
}

// One "ls -l" block. Columns are padded to the widest value so they line up.
// (Not handled: the '+' some ls versions print for files with ACLs/extended attributes.)
void printLong(const std::vector<Entry>& entries, bool showTotal) {
    size_t wLinks = 0, wOwner = 0, wGroup = 0, wSize = 0;
    long long blocks = 0;
    for (const Entry& e : entries) {
        wLinks = std::max(wLinks, std::to_string(e.st.st_nlink).size());
        wOwner = std::max(wOwner, ownerName(e.st.st_uid).size());
        wGroup = std::max(wGroup, groupName(e.st.st_gid).size());
        wSize = std::max(wSize, std::to_string(e.st.st_size).size());
        blocks += e.st.st_blocks;   // counted in 512-byte units
    }
    if (showTotal) printf("total %lld\n", (blocks + 1) / 2);   // ls reports 1K blocks

    for (const Entry& e : entries) {
        printf("%s %*lu %-*s %-*s %*lld %s %s", modeString(e.st.st_mode).c_str(),
               (int)wLinks, (unsigned long)e.st.st_nlink,
               (int)wOwner, ownerName(e.st.st_uid).c_str(),
               (int)wGroup, groupName(e.st.st_gid).c_str(),
               (int)wSize, (long long)e.st.st_size,
               timeString(e.st.st_mtime).c_str(), e.name.c_str());

        if (S_ISLNK(e.st.st_mode)) {   // symlinks show their target
            char target[4096];
            ssize_t n = readlink(e.path.c_str(), target, sizeof target - 1);
            if (n >= 0) {
                target[n] = '\0';
                printf(" -> %s", target);
            }
        }
        putchar('\n');
    }
}

void printEntries(const std::vector<Entry>& entries, bool longFormat, bool showTotal) {
    if (longFormat) {
        printLong(entries, showTotal);
    } else {
        for (const Entry& e : entries) printf("%s\n", e.name.c_str());   // one per line (allowed by spec)
    }
}

// List the contents of one directory. Returns false on error.
bool listDirectory(const std::string& dir, bool showAll, bool longFormat) {
    DIR* d = opendir(dir.c_str());
    if (!d) {
        fprintf(stderr, "ls: cannot open directory '%s': %s\n", dir.c_str(), strerror(errno));
        return false;
    }

    std::vector<Entry> entries;
    while (struct dirent* de = readdir(d)) {
        std::string name = de->d_name;
        if (!showAll && name[0] == '.') continue;   // hidden unless -a

        Entry e;
        e.name = name;
        e.path = dir + "/" + name;
        // lstat (not stat): describe a symlink itself, not what it points to
        if (lstat(e.path.c_str(), &e.st) < 0) {
            fprintf(stderr, "ls: cannot access '%s': %s\n", e.path.c_str(), strerror(errno));
            continue;
        }
        entries.push_back(std::move(e));
    }
    closedir(d);

    std::sort(entries.begin(), entries.end(), byName);   // readdir order is arbitrary
    printEntries(entries, longFormat, true);
    return true;
}

std::string expandTilde(const std::string& p) {
    if (p == "~") return shellHome();
    if (p.rfind("~/", 0) == 0) return shellHome() + p.substr(1);
    return p;
}

}  // namespace

// ls [-a] [-l] [paths...]   flags and paths may come in any order (ls -l d1 -a d2)
int builtin_ls(const std::vector<std::string>& argv) {
    bool showAll = false, longFormat = false;
    std::vector<std::string> paths;

    for (size_t i = 1; i < argv.size(); ++i) {
        const std::string& a = argv[i];
        if (a.size() > 1 && a[0] == '-') {   // "-l", "-a", "-la", "-al"
            for (size_t j = 1; j < a.size(); ++j) {
                if (a[j] == 'a') showAll = true;
                else if (a[j] == 'l') longFormat = true;
                else {
                    fprintf(stderr, "ls: invalid option -- '%c'\n", a[j]);
                    return 2;
                }
            }
        } else {
            paths.push_back(expandTilde(a));
        }
    }
    if (paths.empty()) paths.push_back(".");

    // Like real ls: report missing paths first, then plain files, then directories.
    int status = 0;
    std::vector<Entry> files, dirs;
    for (const std::string& p : paths) {
        Entry e;
        e.name = e.path = p;
        if (lstat(p.c_str(), &e.st) < 0) {
            fprintf(stderr, "ls: cannot access '%s': %s\n", p.c_str(), strerror(errno));
            status = 2;
            continue;
        }
        // A symlink to a directory is listed as a directory unless -l is given.
        struct stat target;
        bool isDir = S_ISDIR(e.st.st_mode) ||
                     (!longFormat && S_ISLNK(e.st.st_mode) && stat(p.c_str(), &target) == 0 &&
                      S_ISDIR(target.st_mode));
        (isDir ? dirs : files).push_back(std::move(e));
    }
    std::sort(files.begin(), files.end(), byName);
    std::sort(dirs.begin(), dirs.end(), byName);

    bool printedSomething = false;
    if (!files.empty()) {
        printEntries(files, longFormat, false);   // no "total" line for plain files
        printedSomething = true;
    }
    const bool showHeaders = paths.size() > 1;
    for (const Entry& d : dirs) {
        if (printedSomething) putchar('\n');
        if (showHeaders) printf("%s:\n", d.name.c_str());
        if (!listDirectory(d.path, showAll, longFormat)) status = 2;
        printedSomething = true;
    }
    return status;
}
