#pragma once
#include <string>
#include <sys/types.h>   // pid_t
#include <vector>

// One process of a job (a job = one pipeline, e.g. "cat f | sort").
struct Proc {
    pid_t pid;
    std::string name;
    bool done = false;      // exited or killed
    bool stopped = false;   // paused by Ctrl-Z / SIGSTOP
};

// Call once at startup: puts the shell in its own process group, takes the
// terminal, ignores Ctrl-C/Ctrl-Z in the shell itself, installs SIGCHLD handler.
void initJobControl();

// In a freshly forked child, before exec: join the job's process group,
// take the terminal if foreground, and restore default signal behaviour.
void prepareChild(pid_t pgid, bool foreground);

// After forking a whole pipeline (all processes share process group `pgid`):
void runInForeground(pid_t pgid, const std::vector<Proc>& procs, const std::string& text);
void addBackgroundJob(pid_t pgid, const std::vector<Proc>& procs, const std::string& text);

// Reap finished/stopped background processes and print messages (before each prompt).
void reportFinishedJobs();
