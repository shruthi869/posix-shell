#include "jobs.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "builtins.h"

// ===========================================================================
// State
// ===========================================================================
struct Job {
    int id = 0;               // the [1], [2] ... number shown to the user
    pid_t pgid = 0;           // process group: every process of the pipeline is in it
    std::vector<Proc> procs;
    std::string text;         // command line, for "jobs"
    bool stopped = false;
};

static std::vector<Job> g_jobs;          // background and stopped jobs
static bool g_interactive = false;       // stdin is a terminal
static pid_t g_shellPgid = 0;
static struct termios g_shellModes;      // terminal settings to restore after a job

static volatile sig_atomic_t g_childChanged = 0;

// The signals a terminal sends because of keys the user presses, plus the
// ones it sends to background groups that touch the terminal.
static const int kJobSignals[] = {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU};

// ===========================================================================
// Setup
// ===========================================================================
static void onSigchld(int) { g_childChanged = 1; }   // async-signal-safe: only a flag

void initJobControl() {
    g_interactive = isatty(STDIN_FILENO);

    if (g_interactive) {
        // If we were started in the background, wait until we are put in the foreground.
        while (tcgetpgrp(STDIN_FILENO) != (g_shellPgid = getpgrp())) kill(-g_shellPgid, SIGTTIN);

        // The shell itself must survive Ctrl-C and Ctrl-Z; only jobs react to them.
        for (int sig : kJobSignals) signal(sig, SIG_IGN);

        // Own process group, and own the terminal.
        if (setpgid(0, 0) < 0 && errno != EPERM) perror("setpgid");   // EPERM: already a group/session leader
        g_shellPgid = getpgrp();
        tcsetpgrp(STDIN_FILENO, g_shellPgid);
        tcgetattr(STDIN_FILENO, &g_shellModes);
    } else {
        g_shellPgid = getpgrp();
    }

    struct sigaction sa {};
    sa.sa_handler = onSigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;   // restart a blocked read instead of failing with EINTR
    if (sigaction(SIGCHLD, &sa, nullptr) < 0) perror("sigaction");
}

void prepareChild(pid_t pgid, bool foreground) {
    pid_t me = getpid();
    if (pgid == 0) pgid = me;   // first process of the pipeline leads the group
    setpgid(me, pgid);
    // Also done by the parent: whichever runs first wins, so there is no race
    // where the program starts reading the terminal before it owns it.
    if (foreground && g_interactive) tcsetpgrp(STDIN_FILENO, pgid);

    // Ignored signals stay ignored across exec, so undo the shell's settings.
    for (int sig : kJobSignals) signal(sig, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

static void giveTerminalTo(pid_t pgid) {
    if (g_interactive) tcsetpgrp(STDIN_FILENO, pgid);
}

static void takeTerminalBack() {
    if (!g_interactive) return;
    tcsetpgrp(STDIN_FILENO, g_shellPgid);
    tcsetattr(STDIN_FILENO, TCSADRAIN, &g_shellModes);   // a program like vim may have changed modes
}

// ===========================================================================
// Job helpers
// ===========================================================================
static bool jobDone(const Job& j) {
    return std::all_of(j.procs.begin(), j.procs.end(), [](const Proc& p) { return p.done; });
}

// Stopped = not finished, and every process that is still alive is paused.
static bool jobStopped(const Job& j) {
    return !jobDone(j) && std::all_of(j.procs.begin(), j.procs.end(),
                                      [](const Proc& p) { return p.done || p.stopped; });
}

static int nextJobId() {
    int id = 0;
    for (const Job& j : g_jobs) id = std::max(id, j.id);
    return id + 1;
}

// Apply one waitpid() result to a process. `report` prints exit messages (background jobs).
static void applyStatus(Proc& p, int status, bool report) {
    if (WIFSTOPPED(status)) {
        p.stopped = true;
    } else if (WIFCONTINUED(status)) {
        p.stopped = false;
    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
        p.done = true;
        if (!report) return;
        if (WIFEXITED(status))
            printf("%s with pid %d exited normally (status %d)\n", p.name.c_str(), p.pid,
                   WEXITSTATUS(status));
        else
            printf("%s with pid %d exited abnormally (signal %d)\n", p.name.c_str(), p.pid,
                   WTERMSIG(status));
    }
}

// Block until every process of the job has finished or stopped.
// WUNTRACED makes waitpid also return when a child is STOPPED (Ctrl-Z).
static void waitForJob(Job& job) {
    bool interruptedByCtrlC = false;
    for (Proc& p : job.procs) {
        while (!p.done && !p.stopped) {
            int status;
            if (waitpid(p.pid, &status, WUNTRACED) < 0) {
                if (errno == EINTR) continue;
                p.done = true;   // ECHILD: already collected
                break;
            }
            applyStatus(p, status, false);
            if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) interruptedByCtrlC = true;
        }
    }
    if (interruptedByCtrlC) putchar('\n');   // the terminal echoed "^C" without a newline
}

// Hand the terminal to the job, wait, take the terminal back.
// If the job was stopped, keep it in the job table.
static void foregroundJob(Job job, bool resume) {
    giveTerminalTo(job.pgid);
    if (resume) {
        for (Proc& p : job.procs) p.stopped = false;
        job.stopped = false;
        kill(-job.pgid, SIGCONT);   // negative pid = send to the whole process group
    }
    waitForJob(job);
    takeTerminalBack();

    if (jobStopped(job)) {
        if (job.id == 0) job.id = nextJobId();
        job.stopped = true;
        printf("\n[%d] Stopped\t%s\n", job.id, job.text.c_str());
        g_jobs.push_back(job);
        std::sort(g_jobs.begin(), g_jobs.end(), [](const Job& a, const Job& b) { return a.id < b.id; });
    }
    fflush(stdout);
}

// ===========================================================================
// Called by the executor
// ===========================================================================
void runInForeground(pid_t pgid, const std::vector<Proc>& procs, const std::string& text) {
    Job job;
    job.pgid = pgid;
    job.procs = procs;
    job.text = text;
    foregroundJob(job, false);
}

void addBackgroundJob(pid_t pgid, const std::vector<Proc>& procs, const std::string& text) {
    Job job;
    job.id = nextJobId();
    job.pgid = pgid;
    job.procs = procs;
    job.text = text;
    g_jobs.push_back(job);
    printf("[%d] %d\n", job.id, procs.back().pid);   // spec: print the PID
    fflush(stdout);
}

void reportFinishedJobs() {
    if (!g_childChanged) return;
    g_childChanged = 0;

    // Signals don't queue: one SIGCHLD may stand for several children, so loop.
    // WNOHANG: don't block. WUNTRACED/WCONTINUED: also notice stop/continue
    // done from outside (e.g. "kill -STOP <pid>" in another terminal).
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (Job& j : g_jobs) {
            for (Proc& p : j.procs) {
                if (p.pid != pid) continue;
                bool wasStopped = j.stopped;
                applyStatus(p, status, true);
                j.stopped = jobStopped(j);
                if (j.stopped && !wasStopped) printf("[%d] Stopped\t%s\n", j.id, j.text.c_str());
            }
        }
    }
    g_jobs.erase(std::remove_if(g_jobs.begin(), g_jobs.end(), jobDone), g_jobs.end());
    fflush(stdout);
}

// ===========================================================================
// Built-ins: jobs, fg, bg
// ===========================================================================
// Accepts "fg", "fg 2" or "fg %2". No number = the most recent job.
static Job* pickJob(const std::vector<std::string>& argv, const char* cmd) {
    if (g_jobs.empty()) {
        fprintf(stderr, "%s: no current job\n", cmd);
        return nullptr;
    }
    if (argv.size() == 1) return &g_jobs.back();
    if (argv.size() > 2) {
        fprintf(stderr, "%s: Invalid arguments\n", cmd);
        return nullptr;
    }
    std::string s = argv[1];
    if (!s.empty() && s[0] == '%') s = s.substr(1);
    char* end = nullptr;
    long id = strtol(s.c_str(), &end, 10);
    for (Job& j : g_jobs)
        if (!s.empty() && *end == '\0' && j.id == id) return &j;
    fprintf(stderr, "%s: %s: no such job\n", cmd, argv[1].c_str());
    return nullptr;
}

int builtin_jobs(const std::vector<std::string>&) {
    reportFinishedJobs();   // don't list jobs that already finished
    for (const Job& j : g_jobs)
        printf("[%d] %s\t%s\n", j.id, j.stopped ? "Stopped" : "Running", j.text.c_str());
    return 0;
}

int builtin_fg(const std::vector<std::string>& argv) {
    Job* j = pickJob(argv, "fg");
    if (!j) return 1;
    Job job = *j;   // take it out of the table while it runs in the foreground
    g_jobs.erase(g_jobs.begin() + (j - g_jobs.data()));
    printf("%s\n", job.text.c_str());
    fflush(stdout);
    foregroundJob(job, true);
    return 0;
}

int builtin_bg(const std::vector<std::string>& argv) {
    Job* j = pickJob(argv, "bg");
    if (!j) return 1;
    if (!j->stopped) {
        fprintf(stderr, "bg: job %d already in background\n", j->id);
        return 1;
    }
    for (Proc& p : j->procs) p.stopped = false;
    j->stopped = false;
    kill(-j->pgid, SIGCONT);   // resume, but don't give it the terminal
    printf("[%d] %s &\n", j->id, j->text.c_str());
    return 0;
}
