#include <iostream>
#include <string>

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

        if (line == "exit") break;   // the parser (next part) will handle spaces/tabs

        std::cout << "you typed: " << line << "\n";   // placeholder until the executor exists
    }
    return 0;
}
