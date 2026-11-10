#include "jobs.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <map>
#include <sys/wait.h>

// pid -> command name, for every background process still running
static std::map<pid_t, std::string> g_bgJobs;

// Set by the signal handler, read by the main loop.
// volatile sig_atomic_t is the only type that is safe to share with a handler.
static volatile sig_atomic_t g_childChanged = 0;

// The kernel sends SIGCHLD to the shell whenever one of its children exits.
// A handler can interrupt the shell at ANY moment (even in the middle of
// printf or a std::map update), so it must only do async-signal-safe work.
// We just raise a flag; the real work happens in reportFinishedJobs().
static void onSigchld(int) {
    g_childChanged = 1;
}

void installChildHandler() {
    struct sigaction sa {};
    sa.sa_handler = onSigchld;
    sigemptyset(&sa.sa_mask);
    // SA_RESTART: if the signal arrives while we are blocked reading input,
    // restart the read instead of failing it with EINTR.
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) < 0) perror("sigaction");
}

void addBackgroundJob(pid_t pid, const std::string& name) {
    g_bgJobs[pid] = name;
}

void reportFinishedJobs() {
    if (!g_childChanged) return;
    g_childChanged = 0;

    // Several children may have exited while we got only one signal
    // (signals of the same type are not queued), so loop until none are left.
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {   // WNOHANG: don't block
        auto it = g_bgJobs.find(pid);
        std::string name = (it != g_bgJobs.end()) ? it->second : "process";

        if (WIFEXITED(status)) {
            printf("%s with pid %d exited normally (status %d)\n", name.c_str(), pid,
                   WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("%s with pid %d exited abnormally (signal %d)\n", name.c_str(), pid,
                   WTERMSIG(status));
        }
        if (it != g_bgJobs.end()) g_bgJobs.erase(it);
    }
    fflush(stdout);
}
