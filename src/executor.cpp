#include "executor.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "jobs.h"

// Run one external program (ls, gedit, sleep, ./a.out ...).
static void runExternal(const Command& cmd, bool background) {
    // Anything still in our output buffer would be copied into the child too.
    std::cout.flush();
    fflush(stdout);

    pid_t pid = fork();   // duplicate this process: returns twice
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        // ---------------- CHILD ----------------
        // execvp wants a NULL-terminated array of C strings.
        std::vector<char*> argv;
        for (const auto& a : cmd.argv) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        // Replace this process's program with the requested one.
        // 'p' = search the PATH directories; 'v' = arguments as an array.
        execvp(argv[0], argv.data());

        // Only reached if exec FAILED (on success exec never returns).
        if (errno == ENOENT)
            fprintf(stderr, "%s: command not found\n", argv[0]);
        else
            fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
        _exit(127);   // _exit, not exit: don't run the shell's cleanup/flush in the child
    }

    // ---------------- PARENT (the shell) ----------------
    if (background) {
        printf("%d\n", pid);          // spec: print the PID of the background process
        fflush(stdout);
        addBackgroundJob(pid, cmd.argv[0]);
        return;                       // don't wait; go straight back to the prompt
    }

    // Foreground: wait for exactly this child to finish.
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {         // EINTR = interrupted by a signal, just retry
            perror("waitpid");
            break;
        }
    }
}

void executeLine(const CommandList& list) {
    for (const Pipeline& p : list) {
        if (p.cmds.size() > 1 || !p.cmds[0].redirs.empty()) {
            std::cerr << "shell: pipes and redirection come in Part 5\n";
            continue;
        }
        runExternal(p.cmds[0], p.background);
    }
}
