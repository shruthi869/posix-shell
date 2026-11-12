# POSIX Shell in C++

An interactive Unix shell written from scratch in C++17 on Linux system calls:
pipelines, I/O redirection, job control (Ctrl-C / Ctrl-Z, `jobs` / `fg` / `bg`),
a raw-mode line editor with history and TAB completion, and built-in `ls`,
`pinfo`, `search` and `history`.

No `system()`, `popen()`, `ncurses` or `<filesystem>` is used.

```
<user@host:~> ls -l src | grep cpp > files.txt ; cat < files.txt | wc -l
<user@host:~> sleep 100
^Z
[1] Stopped	sleep 100
<user@host:~> bg
[1] sleep 100 &
```

## Build and run

Requires Linux (or WSL), `g++` with C++17 and `make`.

```bash
make            # builds ./shell
./shell
make test       # automated tests (51 cases)
make debug      # build with AddressSanitizer + UBSan
make clean
```

## Features

| Feature | Examples |
|---|---|
| Prompt `<user@host:path>` | the start directory is shown as `~`; absolute paths outside it |
| Command lists | `cmd1 ; cmd2`, background with `&` (prints `[job] pid`) |
| External programs | anything in `$PATH`, or `./a.out`; `command not found` / permission errors |
| Built-ins | `cd` (`.` `..` `-` `~` `~/dir`), `pwd`, `echo`, `exit` |
| `ls` | `-a`, `-l`, `-la`/`-al`, flags and paths in any order; output matches the real `ls -l` |
| `pinfo [pid]` | status (`R/S/Z/T`, `+` if foreground), virtual memory, executable path, via `/proc` |
| `search <name>` | recursive search below the current directory, prints `True`/`False` |
| Redirection | `<`, `>`, `>>` (files created with mode 0644); also for built-ins |
| Pipes | any number of stages, combined with redirection: `a < in \| b \| c >> out` |
| Job control | Ctrl-C, Ctrl-Z, `jobs`, `fg [n]`, `bg [n]`, background completion messages |
| Line editor | ↑/↓ history, TAB completion of commands and files, Backspace, Ctrl-C, Ctrl-D |
| `history [n]` | last 10 (or n) of up to 20 commands, saved across sessions in `~/.posix_shell_history` |

## Code map

| File | Responsibility | Assignment requirement |
|---|---|---|
| `src/main.cpp` | main loop: prompt → read → parse → execute | – |
| `src/prompt.cpp` | prompt text, shell home and `~` | 1 |
| `src/parser.cpp` | tokenises a line (`strtok_r`) into pipelines, commands and redirections | 1, 4, 7–9 |
| `src/executor.cpp` | `fork`/`exec`, pipes, redirection, built-ins in or out of the shell | 4, 7–9 |
| `src/jobs.cpp` | process groups, terminal ownership, signals, job table, `jobs`/`fg`/`bg` | 4, 10 |
| `src/input.cpp` | raw-mode line editor, arrow keys | 11, 12 |
| `src/completion.cpp` | TAB completion candidates | 11 |
| `src/history.cpp` | history storage, persistence, `history` | 12 |
| `src/builtins/builtins.cpp` | name → function table, `exit` | 2 |
| `src/builtins/cd.cpp`, `pwd.cpp`, `echo.cpp` | built-ins | 2 |
| `src/builtins/ls.cpp` | `ls -a -l` | 3 |
| `src/builtins/pinfo.cpp` | `pinfo` | 5 |
| `src/builtins/search.cpp` | `search` | 6 |
| `tests/run_tests.sh`, `tests/MANUAL.md` | automated and manual tests | – |

## Design

**Parse, then execute.** `parseLine` turns a line into a `CommandList` (pipelines
separated by `;`/`&`), each `Pipeline` holding `Command`s (argv + redirections) and a
background flag. The executor never parses and the parser never runs anything.

**Built-ins.** Looked up in a hash map. A built-in on its own runs inside the shell
process, which is required for `cd` and `exit`. For redirection the shell saves
stdin/stdout with `dup`, redirects with `dup2`, and restores them afterwards.
Inside a pipeline a built-in runs in a forked child like any other stage.

**Pipelines.** One `pipe()` per connection, created just before the child that writes
into it. Each child `dup2`s its pipe ends onto stdin/stdout, then applies its own file
redirections, then execs. Every unused pipe end is closed in every process (otherwise
readers never see end-of-file), and the shell never holds more than two pipe ends.

**Job control.** The shell puts itself in its own process group, owns the terminal and
ignores SIGINT/SIGTSTP/SIGTTIN/SIGTTOU. Each pipeline gets its own process group
(`setpgid` in both parent and child to avoid a race). A foreground job receives the
terminal with `tcsetpgrp`, so Ctrl-C/Ctrl-Z reach only that job; the shell waits with
`WUNTRACED` so that stopped jobs are noticed, then takes the terminal back and
restores its terminal settings. `fg`/`bg` resume a job with `kill(-pgid, SIGCONT)`.

**Signals.** The SIGCHLD handler only sets a `volatile sig_atomic_t` flag (it may
interrupt the shell at any point, so it does nothing that isn't async-signal-safe).
Before each prompt the shell reaps children with `waitpid(-1, WNOHANG | WUNTRACED |
WCONTINUED)` in a loop, because one signal can stand for several children.

**Line editor.** While reading a line the terminal is in non-canonical mode (no
`ICANON`, `ECHO`, `ISIG`), restored by an RAII object on every return path. Arrow keys
are read as escape sequences. When input is not a terminal, a plain `getline` is used,
so the shell also runs scripts (`./shell < script`).

**History** is a list of at most 20 entries, written after every command to a temporary
file that is then `rename`d over the real one (atomic, so a crash cannot corrupt it).

## Assumptions and limitations

- Quotes and backslash escapes are not interpreted (not required by the assignment):
  `echo "a b"` prints `"a b"`.
- Arguments are split on spaces/tabs; `| ; & < > >>` always act as operators.
- `2>`, `2>&1`, `&&`, `||`, globbing (`*`) and variables (`$HOME`) are not supported.
- `~` means the directory the shell was started from, as the assignment requires.
- Built-ins ignore `&` (the assignment does not require background built-ins).
- The line editor edits at the end of the line only (no left/right cursor movement).
- `ls -l` does not print the `+` some `ls` versions add for files with ACLs/extended attributes.
- TAB completion lists every file in `$PATH` folders without checking the execute bit
  (scanning permissions of thousands of files on WSL's Windows folders took ~9 s).
- If two shells run at once, the last one to save history overwrites the other's.

## Testing

- `make test`: 51 automated cases (parsing errors, built-ins, redirection, pipes,
  background jobs, history across restarts, and `ls` output compared with the real `ls`).
- `make debug && bash tests/run_tests.sh`: the same tests under AddressSanitizer/UBSan.
- Valgrind (`--leak-check=full --track-fds=yes`): no leaks and no leaked descriptors.
- `tests/MANUAL.md`: checklist for the interactive features (Ctrl-C/Z, `fg`/`bg`,
  arrows, TAB).
