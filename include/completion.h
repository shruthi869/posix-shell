#pragma once
#include <string>
#include <vector>

struct Candidate {
    std::string name;
    bool isDir = false;
};

// Works out what TAB should complete at the end of `line`.
//   prefix  : the part of the last word being completed (after its last '/')
//   matches : sorted names that start with prefix
// The first word (or a word after | ; &) completes to commands: built-ins plus
// executables in $PATH. Any other word completes to files and folders.
void findCompletions(const std::string& line, std::string& prefix, std::vector<Candidate>& matches);
