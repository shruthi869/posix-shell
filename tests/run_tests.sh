#!/bin/bash
# Automated tests: feed commands to ./shell through a pipe (non-interactive mode)
# and compare the output with what is expected.
# Interactive features (arrows, TAB, Ctrl-C/Z) are covered by tests/MANUAL.md.
#
# Usage: make test      (or: bash tests/run_tests.sh [path/to/shell])

SHELL_BIN=$(realpath "${1:-./shell}")
[ -x "$SHELL_BIN" ] || { echo "build the shell first (make)"; exit 1; }

WORK=$(mktemp -d)
export HOME="$WORK/home"   # keep the real history file untouched
mkdir -p "$HOME" "$WORK/run"
cd "$WORK/run" || exit 1
export LC_ALL=C

pass=0
fail=0

# check "<name>" "<commands, \n separated>" "<expected output>"
# Prompts and blank lines are removed; process IDs are replaced by "PID".
check() {
    local got
    got=$(printf '%b\nexit\n' "$2" | timeout 10 "$SHELL_BIN" 2>&1 | sed 's/<[^>]*> //g' |
          sed -E 's/^\[([0-9]+)\] [0-9]+$/[\1] PID/; s/pid [0-9]+/pid PID/' | sed '/^$/d')
    if [ "$got" == "$3" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL: $1"
        echo "  expected: $(echo "$3" | tr '\n' '|')"
        echo "  got:      $(echo "$got" | tr '\n' '|')"
    fi
}

# ---------------- parsing ----------------
check "spaces and tabs"        'echo   a \t  b'                 'a b'
check "semicolons"             'echo 1 ; echo 2;echo 3'          $'1\n2\n3'
check "empty segments"         ';; echo x ;'                     'x'
check "blank line"             '   '                             ''
check "pipe at start"          '| ls'                            "shell: missing command before '|'"
check "pipe at end"            'ls |'                            "shell: missing command after '|'"
check "redirect without file"  'echo a >'                        "shell: missing file name after '>'"
check "stray &"                '& ls'                            "shell: syntax error near '&'"

# ---------------- built-ins ----------------
mkdir -p d1/d2
check "pwd"                    'pwd'                             "$WORK/run"
check "cd and pwd"             'cd d1 ; pwd'                     "$WORK/run/d1"
check "cd .. above home"       'cd .. ; pwd'                     "$WORK"
check "cd ~ and cd"            'cd d1/d2 ; cd ~ ; pwd ; cd d1 ; cd ; pwd' "$WORK/run"$'\n'"$WORK/run"
check "cd -"                   'cd d1 ; cd - ; cd -'             "$WORK/run"$'\n'"$WORK/run/d1"
check "cd too many args"       'cd a b'                          'cd: Invalid arguments'
check "cd missing dir"         'cd nope'                         'cd: nope: No such file or directory'
check "echo quotes as-is"      "echo \"abc 'ac' abc\""           "\"abc 'ac' abc\""
check "search found"           'search d2'                       'True'
check "search not found"       'search zzz'                      'False'
check "pinfo bad pid"          'pinfo abc'                       "pinfo: invalid pid 'abc'"
check "pinfo own status"       'pinfo | grep -c Executable'      '1'
check "exit stops the line"    'echo a ; exit ; echo b'          'a'

# ---------------- external commands ----------------
check "external command"       'printf hi\\n'                    'hi'
check "shell inside shell"     "$SHELL_BIN"' < /dev/null ; echo outer-ok' 'outer-ok'
check "command not found"      'nosuchcmd'                       'nosuchcmd: command not found'
printf 'x' > notexec.sh
check "permission denied"      './notexec.sh'                    './notexec.sh: Permission denied'
check "background reaped"      'sleep 0.1 &\nsleep 0.4'          "$(echo '[1] PID'; echo 'sleep with pid PID exited normally (status 0)')"

# ---------------- redirection ----------------
check "> creates, cat reads"   'echo hello > f.txt ; cat f.txt'  'hello'
check ">> appends"             'echo a > g.txt ; echo b >> g.txt ; cat < g.txt' $'a\nb'
check "> truncates"            'echo long > h.txt ; echo s > h.txt ; cat h.txt' 's'
check "file mode 0644"         'echo x > m.txt ; stat -c %a m.txt' '644'
check "missing input file"     'cat < nofile'                    'nofile: No such file or directory'
check "in and out together"    'printf b\\na\\n > u.txt ; sort < u.txt > s.txt ; cat s.txt' $'a\nb'
check "built-in redirected"    'pwd > p.txt ; echo back ; cat p.txt' "back"$'\n'"$WORK/run"

# ---------------- pipes ----------------
check "two-stage pipe"         'echo hello | wc -c'              '6'
check "five-stage pipe"        'echo hi | cat | cat | cat | cat' 'hi'
check "pipe + redirection"     'echo a | cat > q.txt ; cat < q.txt | wc -l' '1'
check "built-in in pipe"       'echo xyz | cat'                  'xyz'
check "cd in pipe stays"       'cd / | cat ; pwd'                "$WORK/run"
check "reader exits first"     'yes | head -2'                   $'y\ny'

# ---------------- history (fresh history file) ----------------
export HOME="$WORK/home2"; mkdir -p "$HOME"
check "history, repeat stored once" 'echo 1\necho 1\nhistory'     $'1\n1\necho 1\nhistory'
check "history survives restart"    'history 3'                   $'history\nexit\nhistory 3'

# ---------------- ls (compared with the real ls) ----------------
touch .hidden a.txt b.txt
for c in "ls" "ls -a" "ls -l" "ls -la" "ls -al" "ls -a -l" "ls -l d1" "ls -l d1 -a ." "ls a.txt d1" "ls -l /etc"; do
    want=$(eval "command $c" 2>&1)
    check "$c (vs real ls)" "$c" "$(echo "$want" | sed '/^$/d')"
done

cd / && rm -rf "$WORK"
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
