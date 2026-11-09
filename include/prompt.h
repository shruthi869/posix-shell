#pragma once
#include <string>

// Remember the directory the shell was started from; it is shown as "~".
void initShellHome();
const std::string& shellHome();

// Current working directory as an absolute path ("?" if getcwd fails).
std::string currentDir();

// Replace the shell-home prefix of an absolute path with "~".
std::string displayPath(const std::string& absPath);

// Builds "<user@host:path> ".
std::string buildPrompt();
