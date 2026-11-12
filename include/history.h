#pragma once
#include <string>
#include <vector>

// Read saved history from the history file (call once at startup).
void loadHistory();

// Record a command line (and save the file). Blank lines and a repeat of
// the previous command are not stored.
void addHistory(const std::string& line);

// Oldest first; at most 20 entries.
const std::vector<std::string>& historyEntries();
