#include "input.h"

#include <cerrno>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include "completion.h"
#include "history.h"

namespace {

// ---------------------------------------------------------------------------
// Raw mode. Normally the terminal driver collects a whole line (with its own
// simple editing) and only hands it over on Enter -- "canonical" mode -- and
// echoes keys itself. To react to single keys (arrows, TAB) we switch both off.
// RAII: the destructor restores the original settings on every return path,
// so commands always run with a normal terminal.
// ---------------------------------------------------------------------------
class RawMode {
public:
    RawMode() {
        ok_ = tcgetattr(STDIN_FILENO, &orig_) == 0;
        if (!ok_) return;
        struct termios raw = orig_;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG);   // byte by byte, no echo, Ctrl-C/Z arrive as bytes
        raw.c_cc[VMIN] = 1;                       // read() returns after 1 byte
        raw.c_cc[VTIME] = 0;                      // no timeout
        // TCSANOW, not TCSAFLUSH: FLUSH would throw away keys typed ahead
        // (e.g. input meant for the next program) while switching modes.
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    ~RawMode() {
        if (ok_) tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
    }

private:
    struct termios orig_;
    bool ok_ = false;
};

void out(const std::string& s) {
    ssize_t r = write(STDOUT_FILENO, s.data(), s.size());
    (void)r;
}

// Returns the next byte, or -1 at end of input.
int readByte() {
    unsigned char c;
    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1) return c;
        if (n < 0 && errno == EINTR) continue;   // interrupted by SIGCHLD: try again
        return -1;
    }
}

// Rewrite the whole line: go to column 0, print prompt + text, clear the rest.
void redraw(const std::string& prompt, const std::string& buf) {
    out("\r" + prompt + buf + "\x1b[K");
}

std::string commonPrefix(const std::vector<Candidate>& m) {
    std::string p = m[0].name;
    for (const Candidate& c : m) {
        size_t i = 0;
        while (i < p.size() && i < c.name.size() && p[i] == c.name[i]) ++i;
        p.resize(i);
    }
    return p;
}

void completeTab(const std::string& prompt, std::string& buf) {
    std::string prefix;
    std::vector<Candidate> matches;
    findCompletions(buf, prefix, matches);

    if (matches.empty()) {
        out("\a");   // bell: nothing matches
        return;
    }
    if (matches.size() == 1) {   // unique: finish the word
        const Candidate& c = matches[0];
        buf += c.name.substr(prefix.size()) + (c.isDir ? "/" : " ");
        redraw(prompt, buf);
        return;
    }
    std::string common = commonPrefix(matches);
    if (common.size() > prefix.size()) {   // several, but they share more letters: fill those in
        buf += common.substr(prefix.size());
        redraw(prompt, buf);
        return;
    }
    // Nothing more to fill in: list all matches, then show the line again.
    std::string list = "\n";
    for (const Candidate& c : matches) list += c.name + (c.isDir ? "/" : "") + "  ";
    out(list + "\n");
    redraw(prompt, buf);
}

}  // namespace

bool readLine(const std::string& prompt, std::string& line) {
    if (!isatty(STDIN_FILENO)) {   // script or pipe: no editing needed
        std::cout << prompt << std::flush;
        return static_cast<bool>(std::getline(std::cin, line));
    }

    std::cout << std::flush;
    RawMode raw;
    out(prompt);

    const std::vector<std::string>& hist = historyEntries();
    size_t histPos = hist.size();   // == size means "the line being typed", not a history entry
    std::string buf, draft;         // draft: what was typed before pressing Up

    while (true) {
        int c = readByte();
        if (c == -1) return false;

        if (c == '\r' || c == '\n') {            // Enter
            out("\n");
            line = buf;
            return true;
        }
        if (c == 4) {                             // Ctrl-D
            if (buf.empty()) {
                out("\n");
                return false;                     // exit the shell
            }
            continue;                             // ignored in the middle of a line
        }
        if (c == 3) {                             // Ctrl-C: throw the line away
            out("^C\n");
            line.clear();
            return true;
        }
        if (c == 127 || c == 8) {                 // Backspace
            if (!buf.empty()) {
                buf.pop_back();
                out("\b \b");                     // left, overwrite with space, left again
            }
            continue;
        }
        if (c == '\t') {
            completeTab(prompt, buf);
            continue;
        }
        if (c == 27) {                            // ESC: start of an arrow-key sequence
            int c1 = readByte();
            if (c1 != '[' && c1 != 'O') continue;
            int c2 = readByte();
            while (c2 >= '0' && c2 <= '9') c2 = readByte();   // e.g. ESC [ 3 ~ (Delete): skip
            if (c2 == 'A' && histPos > 0) {       // Up: older command
                if (histPos == hist.size()) draft = buf;
                buf = hist[--histPos];
                redraw(prompt, buf);
            } else if (c2 == 'B' && histPos < hist.size()) {   // Down: newer command
                ++histPos;
                buf = (histPos == hist.size()) ? draft : hist[histPos];
                redraw(prompt, buf);
            }
            continue;                             // Left/Right and others: ignored
        }
        if (c >= 32) {                            // printable character
            buf += static_cast<char>(c);
            out(std::string(1, static_cast<char>(c)));
        }
        // other control keys (e.g. Ctrl-Z) are ignored at the prompt
    }
}
