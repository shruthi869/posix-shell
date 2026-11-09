#include <iostream>
#include <string>

#include "parser.h"
#include "prompt.h"

// REPL: Read a line, Evaluate it, Print the result, Loop.
int main() {
    initShellHome();

    std::string line;
    while (true) {
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

        if (list[0].cmds[0].argv[0] == "exit") break;   // becomes a proper built-in later

        std::cout << describe(list);   // placeholder until the executor exists
    }
    return 0;
}
