#include "prompt.h"

#include <climits>   // PATH_MAX, HOST_NAME_MAX
#include <cstdio>    // perror
#include <pwd.h>     // getpwuid, struct passwd
#include <unistd.h>  // getcwd, gethostname, getuid

static std::string g_home;   // directory the shell was launched from

std::string currentDir() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof buf) == nullptr) {
        perror("getcwd");
        return "?";
    }
    return buf;
}

void initShellHome() { g_home = currentDir(); }

const std::string& shellHome() { return g_home; }

std::string displayPath(const std::string& p) {
    if (p == g_home) return "~";
    // Only replace a whole directory component: home=/a/proj must not turn
    // /a/projX into ~X. So the character after the prefix must be '/'.
    if (p.size() > g_home.size() && p.compare(0, g_home.size(), g_home) == 0 &&
        p[g_home.size()] == '/') {
        return "~" + p.substr(g_home.size());
    }
    return p;   // outside home: show the absolute path
}

static std::string userName() {
    // getpwuid reads the user database (/etc/passwd); don't trust $USER,
    // because environment variables can be changed by anyone.
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_name : "user";
}

static std::string hostName() {
    char buf[HOST_NAME_MAX + 1];
    if (gethostname(buf, sizeof buf) != 0) return "host";
    buf[HOST_NAME_MAX] = '\0';   // gethostname may not terminate on truncation
    return buf;
}

std::string buildPrompt() {
    return "<" + userName() + "@" + hostName() + ":" + displayPath(currentDir()) + "> ";
}
