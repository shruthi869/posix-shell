#pragma once
#include <string>
#include <vector>

// Every built-in has the same shape: takes argv, returns an exit status (0 = success).
using BuiltinFn = int (*)(const std::vector<std::string>& argv);

// Returns the built-in for this name, or nullptr if it is an external program.
BuiltinFn findBuiltin(const std::string& name);

// Names of all built-ins (used by TAB completion).
const std::vector<std::string>& builtinNames();

// True once the user has run "exit".
bool shellShouldExit();

// The individual built-ins (one file each in src/builtins/).
int builtin_cd(const std::vector<std::string>& argv);
int builtin_pwd(const std::vector<std::string>& argv);
int builtin_echo(const std::vector<std::string>& argv);
int builtin_exit(const std::vector<std::string>& argv);
int builtin_ls(const std::vector<std::string>& argv);
int builtin_pinfo(const std::vector<std::string>& argv);
int builtin_search(const std::vector<std::string>& argv);
int builtin_jobs(const std::vector<std::string>& argv);   // these three live in jobs.cpp
int builtin_fg(const std::vector<std::string>& argv);
int builtin_bg(const std::vector<std::string>& argv);
int builtin_history(const std::vector<std::string>& argv);   // in history.cpp
