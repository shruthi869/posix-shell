# Manual test checklist (interactive features)

These need a real terminal, so they are not in `run_tests.sh`. Run `./shell` and check each item.

## Job control and signals
- [ ] `sleep 30` then **Ctrl-Z** → `[1] Stopped  sleep 30`, prompt returns
- [ ] `jobs` → `[1] Stopped  sleep 30`
- [ ] `bg` → `[1] sleep 30 &`; `jobs` → `Running`
- [ ] `fg` then **Ctrl-C** → job killed, prompt returns, `jobs` is empty
- [ ] **Ctrl-C** at an empty prompt → `^C`, new prompt, shell still alive
- [ ] `sleep 30 | cat` then **Ctrl-Z** → whole pipeline stopped; `fg %1`, **Ctrl-C** kills it
- [ ] `vim` (or `less makefile`) works; **Ctrl-Z** inside it stops it; `fg` brings it back
- [ ] `cat` then type a line then **Ctrl-D** → `cat` ends, prompt returns
- [ ] **Ctrl-D** at an empty prompt → shell exits
- [ ] `pinfo` → status `R+`; `sleep 100 &` then `pinfo <pid>` → `S` (no `+`)
- [ ] `./shell` inside `./shell` works; `exit` returns to the outer one

## Line editing
- [ ] **↑** shows previous commands, stops at the oldest; **↓** goes back to the line being typed
- [ ] A recalled command can be edited before pressing Enter
- [ ] **Backspace** deletes characters
- [ ] `cat alp` + **TAB** (with `alpha.txt`, `alnum.txt` present) → `cat alpha.txt `
- [ ] `cat a` + **TAB** → `cat al`; **TAB** again → lists both files
- [ ] `ec` + **TAB** → lists matching commands (including `echo`)
- [ ] `ls sr` + **TAB** → `ls src/`

## History
- [ ] Run a few commands, `exit`, start the shell again, `history` → old commands still listed
- [ ] `history 3` → only the latest 3
