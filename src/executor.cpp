#include "executor.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>      // open, O_* flags
#include <sys/wait.h>
#include <unistd.h>     // fork, execvp, pipe, dup, dup2, close
#include <vector>

#include "builtins.h"
#include "jobs.h"

// ---------------------------------------------------------------------------
// Redirection: point fd 0 (stdin) or fd 1 (stdout) of THIS process at a file.
//   < file    stdin  <- file   (must exist)
//   > file    stdout -> file   (create with 0644, truncate)
//   >> file   stdout -> file   (create with 0644, append)
// Returns false (after printing why) if a file cannot be opened.
// ---------------------------------------------------------------------------
static bool applyRedirections(const Command& cmd) {
    for (const Redirect& r : cmd.redirs) {
        int fd;
        int target;
        if (r.type == Redirect::IN) {
            fd = open(r.file.c_str(), O_RDONLY);
            target = STDIN_FILENO;
        } else if (r.type == Redirect::OUT) {
            fd = open(r.file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            target = STDOUT_FILENO;
        } else {
            fd = open(r.file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            target = STDOUT_FILENO;
        }
        if (fd < 0) {
            fprintf(stderr, "%s: %s\n", r.file.c_str(), strerror(errno));
            return false;
        }
        // dup2(fd, target): make `target` refer to the same open file as `fd`.
        if (dup2(fd, target) < 0) {
            perror("dup2");
            close(fd);
            return false;
        }
        close(fd);   // the file stays open through `target`; drop the extra copy
    }
    return true;
}

// ---------------------------------------------------------------------------
// In a CHILD process: run a built-in or exec an external program. Never returns.
// ---------------------------------------------------------------------------
[[noreturn]] static void execCommand(const Command& cmd) {
    if (BuiltinFn fn = findBuiltin(cmd.argv[0])) {   // built-in inside a pipeline
        int status = fn(cmd.argv);
        fflush(stdout);
        _exit(status);
    }

    std::vector<char*> argv;
    for (const auto& a : cmd.argv) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());   // only returns on failure

    if (errno == ENOENT)
        fprintf(stderr, "%s: command not found\n", argv[0]);
    else
        fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
    _exit(127);
}

// ---------------------------------------------------------------------------
// A single built-in with no pipe runs in the shell itself (so cd/exit work).
// If it has redirections, temporarily swap the shell's stdin/stdout, then
// put the originals back.
// ---------------------------------------------------------------------------
static void runBuiltinInShell(BuiltinFn fn, const Command& cmd) {
    if (cmd.redirs.empty()) {
        fn(cmd.argv);
        fflush(stdout);
        return;
    }

    fflush(stdout);
    int savedIn = dup(STDIN_FILENO);    // keep copies of the real terminal fds
    int savedOut = dup(STDOUT_FILENO);
    if (savedIn < 0 || savedOut < 0) {
        perror("dup");
        if (savedIn >= 0) close(savedIn);
        if (savedOut >= 0) close(savedOut);
        return;
    }

    if (applyRedirections(cmd)) fn(cmd.argv);
    fflush(stdout);                      // push output into the file before restoring

    dup2(savedIn, STDIN_FILENO);         // restore the terminal
    dup2(savedOut, STDOUT_FILENO);
    close(savedIn);
    close(savedOut);
}

// "cat f | sort > out" -- the text shown by "jobs" and in Stopped messages.
static std::string pipelineText(const Pipeline& p) {
    std::string s;
    for (size_t i = 0; i < p.cmds.size(); ++i) {
        if (i > 0) s += " | ";
        for (size_t k = 0; k < p.cmds[i].argv.size(); ++k) s += (k ? " " : "") + p.cmds[i].argv[k];
        for (const Redirect& r : p.cmds[i].redirs)
            s += (r.type == Redirect::IN ? " < " : r.type == Redirect::OUT ? " > " : " >> ") + r.file;
    }
    return s;
}

// ---------------------------------------------------------------------------
// Run "cmd1 | cmd2 | ... | cmdN" (N may be 1).
//
// For each command i:
//   - create a pipe to the NEXT command (except for the last one)
//   - fork; in the child:
//       stdin  <- read end of the previous pipe   (if not the first)
//       stdout -> write end of the new pipe       (if not the last)
//       then apply its own < > >> (these override the pipe), then exec
//   - in the parent: close the pipe ends it no longer needs
//
// Only two pipe fds are ever open in the shell at once, so N can be anything.
// ---------------------------------------------------------------------------
static void runPipeline(const Pipeline& p) {
    const size_t n = p.cmds.size();

    if (n == 1) {
        if (BuiltinFn fn = findBuiltin(p.cmds[0].argv[0])) {
            runBuiltinInShell(fn, p.cmds[0]);   // '&' ignored for built-ins (spec)
            return;
        }
    }

    fflush(stdout);   // don't let buffered output get duplicated into children

    std::vector<Proc> procs;
    pid_t pgid = 0;      // process group of this pipeline = PID of its first process
    int prevRead = -1;   // read end of the pipe from the previous command

    for (size_t i = 0; i < n; ++i) {
        const bool last = (i == n - 1);
        int fds[2] = {-1, -1};   // fds[0] = read end, fds[1] = write end

        if (!last && pipe(fds) < 0) {
            perror("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (!last) { close(fds[0]); close(fds[1]); }
            break;
        }

        if (pid == 0) {
            // ---------------- CHILD i ----------------
            prepareChild(pgid, !p.background);    // own process group, default signals
            if (prevRead != -1) {                 // read from the previous command
                dup2(prevRead, STDIN_FILENO);
                close(prevRead);
            }
            if (!last) {                          // write to the next command
                close(fds[0]);                    // child i never reads its own pipe
                dup2(fds[1], STDOUT_FILENO);
                close(fds[1]);
            }
            if (!applyRedirections(p.cmds[i])) _exit(1);
            execCommand(p.cmds[i]);
        }

        // ---------------- PARENT ----------------
        if (pgid == 0) pgid = pid;                // first child leads the group
        setpgid(pid, pgid);                       // also set here: avoids a race with the child
        procs.push_back({pid, p.cmds[i].argv[0]});
        if (prevRead != -1) {
            close(prevRead);                      // child i now owns it
            prevRead = -1;                        // never close the same number twice
        }
        if (!last) {
            close(fds[1]);                        // shell never writes into pipes
            prevRead = fds[0];                    // next child will read this
        }
    }
    if (prevRead != -1) close(prevRead);          // only if we stopped early

    if (procs.empty()) return;

    if (p.background)
        addBackgroundJob(pgid, procs, pipelineText(p));   // prints "[1] <pid>", no waiting
    else
        runInForeground(pgid, procs, pipelineText(p));    // give terminal, wait, take it back
}

void executeLine(const CommandList& list) {
    for (const Pipeline& p : list) {
        runPipeline(p);
        if (shellShouldExit()) return;   // don't run anything after "exit"
    }
}
