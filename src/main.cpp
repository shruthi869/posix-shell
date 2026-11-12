#include <iostream>
#include <string>
#include <unistd.h>   // isatty

#include "builtins.h"
#include "executor.h"
#include "history.h"
#include "input.h"
#include "jobs.h"
#include "parser.h"
#include "prompt.h"

// REPL: Read a line, Evaluate it, Print the result, Loop.
int main() {
    initShellHome();
    initJobControl();
    loadHistory();

    std::string line;
    while (true) {
        reportFinishedJobs();   // "gedit with pid 1234 exited normally"

        if (!readLine(buildPrompt(), line)) {   // Ctrl-D on an empty line, or end of a script
            if (!isatty(STDIN_FILENO)) std::cout << "\n";
            break;
        }

        addHistory(line);   // before running, so "history" lists itself (as in the spec)

        CommandList list;
        std::string err;
        if (!parseLine(line, list, err)) {
            std::cerr << "shell: " << err << "\n";
            continue;
        }
        if (list.empty()) continue;   // blank line or only spaces

        executeLine(list);
        if (shellShouldExit()) break;
    }
    return 0;
}
