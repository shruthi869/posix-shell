#pragma once
#include <string>
#include <vector>

// One redirection, e.g. "> out.txt"
struct Redirect {
    enum Type { IN, OUT, APPEND } type;   // <   >   >>
    std::string file;
};

// One program to run, e.g. "grep -i abc < in.txt"
struct Command {
    std::vector<std::string> argv;        // {"grep", "-i", "abc"}
    std::vector<Redirect> redirs;         // {IN, "in.txt"}
};

// Commands joined by '|', e.g. "cat f | sort | uniq", optionally ending in '&'
struct Pipeline {
    std::vector<Command> cmds;
    bool background = false;
};

// Everything on one input line: pipelines separated by ';' or '&'
using CommandList = std::vector<Pipeline>;

// Parses one input line. Returns false and sets err on a syntax error.
bool parseLine(const std::string& line, CommandList& out, std::string& err);
