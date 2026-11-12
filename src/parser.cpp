#include "parser.h"

#include <cstring>   // strtok_r

// ---------------------------------------------------------------------------
// Step 1: put spaces around operators, so "ls>out|wc" becomes "ls > out | wc".
// After this, every token is separated by whitespace.
// ---------------------------------------------------------------------------
static std::string spaceOperators(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '>' && i + 1 < s.size() && s[i + 1] == '>') {   // ">>" is one operator
            r += " >> ";
            ++i;
        } else if (c == '|' || c == ';' || c == '&' || c == '<' || c == '>') {
            r += ' ';
            r += c;
            r += ' ';
        } else {
            r += c;
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// Step 2: split on spaces and tabs with strtok_r.
// strtok modifies the string it scans, so we give it a private copy.
// strtok_r (not strtok) keeps its position in `save` instead of a hidden
// global, so it is safe to nest or call from several places.
// ---------------------------------------------------------------------------
static std::vector<std::string> tokenize(const std::string& s) {
    std::vector<char> buf(s.begin(), s.end());
    buf.push_back('\0');

    std::vector<std::string> tokens;
    char* save = nullptr;
    for (char* t = strtok_r(buf.data(), " \t\r\n", &save); t != nullptr;
         t = strtok_r(nullptr, " \t\r\n", &save)) {
        tokens.emplace_back(t);
    }
    return tokens;
}

static bool isOperator(const std::string& t) {
    return t == "|" || t == ";" || t == "&" || t == "<" || t == ">" || t == ">>";
}

// ---------------------------------------------------------------------------
// Step 3: walk the tokens left to right and build the structure.
// ---------------------------------------------------------------------------
bool parseLine(const std::string& line, CommandList& out, std::string& err) {
    out.clear();
    std::vector<std::string> tokens = tokenize(spaceOperators(line));

    Pipeline pipe;   // pipeline being built
    Command cmd;     // command being built

    // Called at ';', '&' or end of line: closes the current pipeline.
    auto finishPipeline = [&](bool background) -> bool {
        if (cmd.argv.empty()) {
            if (!pipe.cmds.empty())  { err = "missing command after '|'"; return false; }
            if (!cmd.redirs.empty()) { err = "missing command before redirection"; return false; }
            if (background)          { err = "syntax error near '&'"; return false; }
            return true;   // empty segment like "ls ;" or ";;" -- just skip it
        }
        pipe.cmds.push_back(std::move(cmd));
        cmd = Command{};
        pipe.background = background;
        out.push_back(std::move(pipe));
        pipe = Pipeline{};
        return true;
    };

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];

        if (t == "|") {
            if (cmd.argv.empty()) { err = "missing command before '|'"; return false; }
            pipe.cmds.push_back(std::move(cmd));
            cmd = Command{};
        } else if (t == ";" || t == "&") {
            if (!finishPipeline(t == "&")) return false;
        } else if (t == "<" || t == ">" || t == ">>") {
            if (i + 1 >= tokens.size() || isOperator(tokens[i + 1])) {
                err = "missing file name after '" + t + "'";
                return false;
            }
            Redirect::Type type = (t == "<") ? Redirect::IN
                                : (t == ">") ? Redirect::OUT
                                             : Redirect::APPEND;
            cmd.redirs.push_back({type, tokens[++i]});   // ++i: the file name is consumed
        } else {
            cmd.argv.push_back(t);   // ordinary word: program name or argument
        }
    }
    return finishPipeline(false);
}
