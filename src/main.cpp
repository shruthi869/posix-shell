#include <iostream>
#include <string>

#include "builtins.h"
#include "executor.h"
#include "jobs.h"
#include "parser.h"
#include "prompt.h"

// REPL: Read a line, Evaluate it, Print the result, Loop.
int main() {
    initShellHome();
    installChildHandler();

    std::string line;
    while (true) {
        reportFinishedJobs();   // "gedit with pid 1234 exited normally"

        std::cout << buildPrompt() << std::flush;   // flush: no newline after the prompt

        if (!std::getline(std::cin, line)) {   // EOF, i.e. Ctrl-D on an empty line
            std::cout << "\n";
            break;
        }

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
