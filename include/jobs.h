#pragma once
#include <string>
#include <sys/types.h>   // pid_t

// Install the SIGCHLD handler (call once at startup).
void installChildHandler();

// Remember a background process so we can report when it finishes.
void addBackgroundJob(pid_t pid, const std::string& name);

// Reap finished background processes and print a message for each.
// Called from the main loop just before printing the prompt.
void reportFinishedJobs();
