#pragma once
#include <string>
#include <vector>

// Every built-in has the same shape: takes argv, returns an exit status (0 = success).
using BuiltinFn = int (*)(const std::vector<std::string>& argv);

// Returns the built-in for this name, or nullptr if it is an external program.
BuiltinFn findBuiltin(const std::string& name);

// True once the user has run "exit".
bool shellShouldExit();

// The individual built-ins (one file each in src/builtins/).
int builtin_cd(const std::vector<std::string>& argv);
int builtin_pwd(const std::vector<std::string>& argv);
int builtin_echo(const std::vector<std::string>& argv);
int builtin_exit(const std::vector<std::string>& argv);
