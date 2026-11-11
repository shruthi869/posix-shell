#include <cerrno>
#include <climits>    // PATH_MAX
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>   // getpid, readlink

#include "builtins.h"
#include "prompt.h"   // displayPath (shows "~" for paths under the shell home)

// pinfo [pid] -- process info read from the /proc filesystem (popen is not allowed).
//   /proc/<pid>/stat    one line: pid (name) state ppid pgrp session tty tpgid ...
//   /proc/<pid>/status  "Key:  value" lines, including VmSize (virtual memory, kB)
//   /proc/<pid>/exe     a symlink to the program file the process is running
int builtin_pinfo(const std::vector<std::string>& argv) {
    if (argv.size() > 2) {
        fprintf(stderr, "pinfo: Invalid arguments\n");
        return 1;
    }

    std::string pid = (argv.size() == 2) ? argv[1] : std::to_string(getpid());
    if (pid.find_first_not_of("0123456789") != std::string::npos) {
        fprintf(stderr, "pinfo: invalid pid '%s'\n", pid.c_str());
        return 1;
    }
    const std::string proc = "/proc/" + pid;

    // ---- Status, from /proc/<pid>/stat ----
    std::ifstream statFile(proc + "/stat");
    std::string statLine;
    if (!statFile || !std::getline(statFile, statLine)) {
        fprintf(stderr, "pinfo: no process with pid %s\n", pid.c_str());
        return 1;
    }
    // The name is in parentheses and may itself contain spaces or ')',
    // so continue parsing after the LAST ')'.
    size_t close = statLine.rfind(')');
    if (close == std::string::npos) {
        fprintf(stderr, "pinfo: cannot parse %s/stat\n", proc.c_str());
        return 1;
    }
    std::istringstream rest(statLine.substr(close + 2));
    char state = '?';
    long ppid = 0, pgrp = 0, session = 0, tty = 0, tpgid = 0;
    rest >> state >> ppid >> pgrp >> session >> tty >> tpgid;

    // '+' = in the foreground: its process group is the one that currently
    // owns its terminal (tpgid). -1 means it has no terminal at all.
    std::string status(1, state);
    if (tpgid != -1 && pgrp == tpgid) status += '+';

    // ---- Virtual memory, from /proc/<pid>/status ----
    std::ifstream statusFile(proc + "/status");
    std::string line, vmSize = "0";   // zombies and kernel threads have no VmSize
    while (std::getline(statusFile, line)) {
        if (line.rfind("VmSize:", 0) == 0) {
            std::istringstream in(line.substr(7));
            in >> vmSize;   // number in kB
            break;
        }
    }

    // ---- Executable path, from the /proc/<pid>/exe symlink ----
    char exe[PATH_MAX];
    ssize_t n = readlink((proc + "/exe").c_str(), exe, sizeof exe - 1);
    std::string exePath;
    if (n >= 0) {
        exe[n] = '\0';
        exePath = displayPath(exe);
    } else {
        exePath = std::string("(unavailable: ") + strerror(errno) + ")";   // e.g. another user's process
    }

    printf("pid -- %s\n", pid.c_str());
    printf("Process Status -- %s\n", status.c_str());
    printf("memory -- %s {Virtual Memory}\n", vmSize.c_str());
    printf("Executable Path -- %s\n", exePath.c_str());
    return 0;
}
