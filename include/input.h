#pragma once
#include <string>

// Shows `prompt` and reads one line from the user.
// On a terminal: raw-mode line editor with Up/Down history, TAB completion,
// Backspace, Ctrl-C (discard line) and Ctrl-D (exit on an empty line).
// Otherwise (input from a file or pipe): a plain getline.
// Returns false at end of input (Ctrl-D / end of file).
bool readLine(const std::string& prompt, std::string& line);
