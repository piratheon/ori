#include "ori_core.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstdint>
#include <algorithm>
#include <utility>

#ifndef ORI_VERSION
#define ORI_VERSION "0.0"
#endif

std::atomic<bool> keep_running{true};
bool g_is_gui_mode = false;
bool g_is_interactive_mode = false;
bool g_debug_enabled_in_gui_mode = false; // New global flag for GUI debug logging
std::atomic<bool> OriAssistant::interrupted_flag{false};

void sigint_handler(int signum) {
    if (g_is_gui_mode) {
        exit(0); // Terminate the process if in GUI mode
    }
    OriAssistant::interrupted_flag = true;
}

void run_spinner(const std::string& message) {
    if (!g_is_interactive_mode || g_is_gui_mode) {
        return;
    }
    const std::vector<std::string> frames = {
        "⠾", "⠽", "⠻", "⠯", "⠷"
    };
    const int fps = 12;
    size_t i = 0;
    std::cout << "\x1b[?25l"; 
    while (keep_running) {
        std::cout << "\r" << frames[i] << " " << message << std::flush;
        i = (i + 1) % frames.size();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
    }
    std::cout << "\r\x1b[2K\x1b[?25h";
}

#include <cstdlib>

bool isGuiEnvironment() {
    // X11 (or XWayland) sets DISPLAY; pure Wayland sessions only set WAYLAND_DISPLAY.
    const char* display = std::getenv("DISPLAY");
    if (display != nullptr && display[0] != '\0') return true;
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    return wayland != nullptr && wayland[0] != '\0';
}

bool has_pkexec() {
    static int cached_status = -1;
    if (cached_status == -1) {
        cached_status = (std::system("which pkexec > /dev/null 2>&1") == 0) ? 1 : 0;
    }
    return cached_status == 1;
}

std::string prepare_elevated_command(const std::string& cmd) {
    // pkexec needs a polkit authentication agent, which only exists in a graphical
    // session. On a headless TTY or over SSH it is installed but unusable, so keep
    // sudo there.
    if (!has_pkexec() || !isGuiEnvironment()) {
        return cmd;
    }
    std::string result = cmd;
    size_t pos = 0;
    while ((pos = result.find("sudo", pos)) != std::string::npos) {
        bool left_boundary = (pos == 0 || isspace((unsigned char)result[pos - 1]) || result[pos - 1] == ';' || result[pos - 1] == '&' || result[pos - 1] == '|');
        bool right_boundary = (pos + 4 == result.length() || isspace((unsigned char)result[pos + 4]));
        if (left_boundary && right_boundary) {
            result.replace(pos, 4, "pkexec");
            pos += 6;
        } else {
            pos += 4;
        }
    }
    return result;
}


// ---------------------------------------------------------------------------
// Git helpers used by the backup / undo machinery.
//
// Everything here is spawned with an argv vector (fork + execvp), never through
// a shell, so file names supplied by the model can not be interpreted as shell
// syntax or git options.
// ---------------------------------------------------------------------------
namespace fs = std::filesystem;

namespace {

struct ProcResult {
    bool started = false;
    int exit_code = -1;
    std::string out;
    bool ok() const { return started && exit_code == 0; }
};

std::string trimEnd(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    return s;
}

using EnvList = std::vector<std::pair<std::string, std::string>>;

ProcResult runProcess(const std::vector<std::string>& argv, const EnvList& env = {}) {
    ProcResult result;
    if (argv.empty()) return result;

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) return result;

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return result;
    }

    if (pid == 0) { // child
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        for (const auto& kv : env) {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }
        std::vector<char*> args;
        args.reserve(argv.size() + 1);
        for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }

    close(pipe_fd[1]);
    char buf[4096];
    while (true) {
        ssize_t n = read(pipe_fd[0], buf, sizeof(buf));
        if (n > 0) {
            result.out.append(buf, static_cast<size_t>(n));
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    close(pipe_fd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
    result.started = true;
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

// Runs `git [-C repo_root] <args...>`. Pathspecs are always taken literally so a
// file called "*.txt" is never expanded into a glob.
ProcResult runGit(const std::string& repo_root, const std::vector<std::string>& args, EnvList env = {}) {
    std::vector<std::string> argv = {"git"};
    if (!repo_root.empty()) {
        argv.push_back("-C");
        argv.push_back(repo_root);
    }
    argv.insert(argv.end(), args.begin(), args.end());
    env.push_back({"GIT_LITERAL_PATHSPECS", "1"});
    env.push_back({"GIT_TERMINAL_PROMPT", "0"});
    return runProcess(argv, env);
}

bool gitAvailable() {
    static int cached = -1;
    if (cached == -1) {
        cached = runProcess({"git", "--version"}).ok() ? 1 : 0;
    }
    return cached == 1;
}

// A repository rooted at "/" or at the user's home directory is never a project
// Ori may snapshot or initialize: staging it would sweep up personal files.
bool isUnsafeRoot(const std::string& dir) {
    std::error_code ec;
    if (dir.empty()) return true;
    if (fs::equivalent(dir, "/", ec) && !ec) return true;
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        ec.clear();
        if (fs::equivalent(dir, home, ec) && !ec) return true;
    }
    return false;
}

struct RepoInfo {
    std::string root;    // work tree top level
    std::string git_dir; // absolute git dir
};

// Locates the git work tree containing the current directory.
bool locateRepo(RepoInfo& info) {
    if (!gitAvailable()) return false;
    ProcResult inside = runGit("", {"rev-parse", "--is-inside-work-tree"});
    if (!inside.ok() || trimEnd(inside.out) != "true") return false;
    ProcResult top = runGit("", {"rev-parse", "--show-toplevel"});
    if (!top.ok()) return false;
    info.root = trimEnd(top.out);
    if (info.root.empty()) return false;
    ProcResult gd = runGit(info.root, {"rev-parse", "--absolute-git-dir"});
    if (!gd.ok()) return false;
    info.git_dir = trimEnd(gd.out);
    return !info.git_dir.empty();
}

// Marker file created by /init. It lives inside the git dir so it never shows up
// in the working tree, is never committed, and is local to this clone.
fs::path projectMarkerPath(const RepoInfo& info) {
    return fs::path(info.git_dir) / "ori" / "project.json";
}

enum class BackupBlock { None, NoGit, NotRepo, UnsafeRoot, NotInitialized };

BackupBlock probeBackup(RepoInfo& info) {
    if (!gitAvailable()) return BackupBlock::NoGit;
    if (!locateRepo(info)) return BackupBlock::NotRepo;
    if (isUnsafeRoot(info.root)) return BackupBlock::UnsafeRoot;
    std::error_code ec;
    if (!fs::exists(projectMarkerPath(info), ec)) return BackupBlock::NotInitialized;
    return BackupBlock::None;
}

} // namespace

// ANSI Color Codes
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";

#ifdef CURL_FOUND
#include <curl/curl.h>
#include <iomanip>

// Callback function to write response data to a string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}
#endif
#include <json/json.h>
#include <dirent.h>
#include "ori_edit.h"

std::string OriAssistant::readInput() {
    const std::string prompt = "> ";
    std::string buffer;
    size_t cursor = 0;

    // Save terminal state
    struct termios orig_termios;
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        // fallback to simple getline
        std::string line;
        if (!std::getline(std::cin, line)) {
            return "";
        }
        return line;
    }

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    auto refresh = [&]() {
        // Move to start of input area
        printf("\r");
        
        // Calculate cursor position
        size_t cursor_line = 0;
        size_t cursor_col = 0;
        
        // Count lines before cursor
        for (size_t i = 0; i < cursor; i++) {
            if (buffer[i] == '\n') {
                cursor_line++;
                cursor_col = 0;
            } else {
                cursor_col++;
            }
        }
        
        // Clear from cursor to end of screen
        printf("\033[J");
        
        // Print buffer contents with line tracking
        size_t current_line = 0;
        
        // Print first prompt and first line
        printf("%s", prompt.c_str());
        size_t i = 0;
        while (i < buffer.size() && buffer[i] != '\n') {
            putchar(buffer[i++]);
        }
        
        // Print remaining lines
        while (i < buffer.size()) {
            if (buffer[i] == '\n') {
                putchar('\n');
                printf("%s", prompt.c_str());
                current_line++;
                i++;
                // Print rest of the line
                while (i < buffer.size() && buffer[i] != '\n') {
                    putchar(buffer[i++]);
                }
            }
        }
        
        // Return cursor to correct position
        if (current_line > cursor_line) {
            printf("\033[%zuA", current_line - cursor_line);
        }
        
        // Set correct column position
        printf("\r");
        if (cursor_col > 0 || prompt.size() > 0) {
            printf("\033[%zuC", prompt.size() + cursor_col);
        }
        
        fflush(stdout);
    };

    refresh();

    while (true) {
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) <= 0) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            std::cin.setstate(std::ios::eofbit);
            printf("\n");
            return "";
        }

        if (c == '\r' || c == '\n') {
            // Ensure terminal state is restored first
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            // Trim any trailing CR/LF characters that may have been
            // inserted into the buffer by the terminal or input flow.
            while (!buffer.empty() && (buffer.back() == '\r' || buffer.back() == '\n')) {
                buffer.pop_back();
            }
            printf("\n");  // Move to next line
            return buffer;
        } else if (c == 0x7f || c == 8) { // Backspace
            if (cursor > 0) {
                buffer.erase(cursor - 1, 1);
                cursor--;
            }
            refresh();
        } else if (c == 0x03) { // Ctrl-C
            buffer.clear();
            cursor = 0;
            printf("\n");
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            return std::string();
        } else if (c == 0x04) { // Ctrl-D
            if (buffer.empty()) {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
                std::cin.setstate(std::ios::eofbit);
                printf("\n");
                exit(0);
            }
        } else if (c == 0x01) { // Ctrl-A -> start
            cursor = 0;
            refresh();
        } else if (c == 0x05) { // Ctrl-E -> end
            cursor = buffer.size();
            refresh();
        } else if (c == 0x06) { // Ctrl-F
            show_command_log = !show_command_log;
            if (!config.no_clear) {
                std::system("clear");
            }
            showBanner();
            if (show_command_log) {
                displayCommandLog();
            }
            refresh();
        } else if (c == 0x15) { // Ctrl-U -> delete to start
            buffer.erase(0, cursor);
            cursor = 0;
            refresh();
        } else if (c == 0x17) { // Ctrl-W -> delete previous word
            if (cursor == 0) { refresh(); continue; }
            size_t i = cursor;
            while (i > 0 && buffer[i-1] == ' ') i--;
            while (i > 0 && buffer[i-1] != ' ') i--;
            buffer.erase(i, cursor - i);
            cursor = i;
            refresh();
        } else if (c == 0x1b) { // ESC sequences (arrows / Alt+key word movement)
            char c2 = 0;
            if (read(STDIN_FILENO, &c2, 1) <= 0) { // Lone ESC
                buffer.clear();
                cursor = 0;
                printf("\n");
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
                return std::string();
            }

            // Handle CSI sequences (arrow keys, Home/End, etc.)
            if (c2 == '[') {
                char c3 = 0;
                if (read(STDIN_FILENO, &c3, 1) <= 0) continue;
                if (c3 >= '0' && c3 <= '9') {
                    std::string num;
                    num.push_back(c3);
                    char c4 = 0;
                    while (read(STDIN_FILENO, &c4, 1) > 0) {
                        if (c4 == '~') break;
                        num.push_back(c4);
                    }
                    if (num == "1") cursor = 0;
                    else if (num == "4" || num == "7") cursor = buffer.size();
                    refresh();
                    continue;
                } else {
                    if (c3 == 'D') { // Left
                        if (cursor > 0) cursor--;
                        refresh();
                        continue;
                    } else if (c3 == 'C') { // Right
                        if (cursor < buffer.size()) cursor++;
                        refresh();
                        continue;
                    } else if (c3 == 'H') { // Home
                        cursor = 0; refresh(); continue;
                    } else if (c3 == 'F') { // End
                        cursor = buffer.size(); refresh(); continue;
                    }
                }
            } else {
                // Alt+<key> sequences: support Alt+b / Alt+f for word movement
                if (c2 == 'b') { // word left (Alt+b)
                    if (cursor == 0) { refresh(); continue; }
                    size_t i = cursor;
                    while (i > 0 && buffer[i-1] == ' ') i--;
                    while (i > 0 && buffer[i-1] != ' ') i--;
                    cursor = i;
                    refresh();
                    continue;
                } else if (c2 == 'f') { // word right (Alt+f)
                    size_t i = cursor;
                    while (i < buffer.size() && buffer[i] != ' ') i++;
                    while (i < buffer.size() && buffer[i] == ' ') i++;
                    cursor = i;
                    refresh();
                    continue;
                }
            }
        } else if (c == '\t') { // Tab autocompletion
            std::vector<std::string> slash_commands = {
                "/help", "/quit", "/exit", "/clear", "/cat", "/exec",
                "/autoexec", "/model", "/thinking", "/agents", "/task", "/subagent",
                "/undo", "/init", "/cmdoutput", "/gitbackup", "/skills", "/skill", "/rag"
            };
            if (buffer.rfind("/task", 0) == 0) {
                slash_commands = {"/task list", "/task clear", "/task run", "/task next", "/task create ", "/task decompose "};
            } else if (buffer.rfind("/subagent", 0) == 0) {
                slash_commands = {"/subagent list"};
            }

            if (buffer.rfind("/cat ", 0) == 0) {
                std::string prefix = buffer.substr(5);
                std::string dir_path = ".";
                std::string file_prefix = prefix;
                size_t last_slash = prefix.find_last_of('/');
                if (last_slash != std::string::npos) {
                    dir_path = prefix.substr(0, last_slash);
                    if (dir_path.empty()) dir_path = "/";
                    file_prefix = prefix.substr(last_slash + 1);
                }

                std::vector<std::string> matches;
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
                        std::string filename = entry.path().filename().string();
                        if (filename.rfind(file_prefix, 0) == 0) {
                            std::string match = (dir_path == "." ? "" : (dir_path == "/" ? "/" : dir_path + "/")) + filename;
                            if (entry.is_directory()) match += "/";
                            matches.push_back(match);
                        }
                    }
                } catch (...) {}

                if (matches.size() == 1) {
                    buffer = "/cat " + matches[0];
                    cursor = buffer.size();
                } else if (matches.size() > 1) {
                    std::string common = matches[0];
                    for (size_t k = 1; k < matches.size(); ++k) {
                        size_t j = 0;
                        while (j < common.size() && j < matches[k].size() && common[j] == matches[k][j]) j++;
                        common = common.substr(0, j);
                    }
                    if (common.size() > file_prefix.size()) {
                        buffer = "/cat " + common;
                        cursor = buffer.size();
                    }
                }
            } else if (buffer.empty() || buffer[0] == '/') {
                std::vector<std::string> matches;
                for (const auto& cmd : slash_commands) {
                    if (cmd.rfind(buffer, 0) == 0) {
                        matches.push_back(cmd);
                    }
                }
                if (matches.size() == 1) {
                    buffer = matches[0];
                    cursor = buffer.size();
                } else if (matches.size() > 1) {
                    std::string common = matches[0];
                    for (size_t k = 1; k < matches.size(); ++k) {
                        size_t j = 0;
                        while (j < common.size() && j < matches[k].size() && common[j] == matches[k][j]) j++;
                        common = common.substr(0, j);
                    }
                    if (common.size() > buffer.size()) {
                        buffer = common;
                        cursor = buffer.size();
                    }
                }
            }
            refresh();
        } else if (isprint(static_cast<unsigned char>(c))) {
            buffer.insert(buffer.begin() + cursor, c);
            cursor++;
            refresh();
        }
    }
}





std::string colorize(const std::string& color, const std::string& text) {
    if (g_is_gui_mode) {
        return text;
    }
    return color + text + RESET;
}

void OriAssistant::setSystemPrompt(const std::string& prompt) {
    conversation_history.push_back({"system", prompt});
}

bool OriAssistant::gitBackupCommit(std::string& out_commit_hash, std::string* out_ref, std::string* out_root) {
    out_commit_hash.clear();
    if (out_ref) out_ref->clear();
    if (out_root) out_root->clear();
    if (!config.git_backup_enabled) {
        return false;
    }

    // Never `git init` here: snapshots are only taken in a repository the user has
    // explicitly initialized with /init, and never for "/" or the home directory.
    RepoInfo repo;
    BackupBlock block = probeBackup(repo);
    if (block != BackupBlock::None) {
        if (!git_hint_shown) {
            if (block == BackupBlock::NotRepo || block == BackupBlock::NotInitialized) {
                git_hint_shown = true;
                std::cerr << YELLOW << "[i] Pre-edit git snapshots are off here (not an initialized Ori project). "
                          << "Run /init (or `ori --init`) to enable /undo for file changes." << RESET << std::endl;
            } else if (block == BackupBlock::UnsafeRoot) {
                git_hint_shown = true;
                std::cerr << YELLOW << "[!] Refusing to snapshot a repository rooted at your home directory or '/'."
                          << RESET << std::endl;
            }
        }
        return false;
    }

    if (backup_session_id.empty()) {
        backup_session_id = std::to_string(static_cast<long long>(getpid())) + "-" +
            std::to_string(static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
    }
    if (!backup_refs_pruned) {
        backup_refs_pruned = true;
        pruneStaleBackupRefs(repo.root);
    }

    // Build the snapshot in a throw-away index so the user's real index, HEAD,
    // branch and git config are never modified.
    std::error_code ec;
    fs::create_directories(fs::path(repo.git_dir) / "ori", ec);
    const std::string tmp_index = (fs::path(repo.git_dir) / "ori" /
        ("index.tmp." + std::to_string(static_cast<long long>(getpid())))).string();
    fs::remove(tmp_index, ec);

    EnvList env = {
        {"GIT_INDEX_FILE", tmp_index},
        {"GIT_AUTHOR_NAME", "Ori Assistant"},
        {"GIT_AUTHOR_EMAIL", "ori@local"},
        {"GIT_COMMITTER_NAME", "Ori Assistant"},
        {"GIT_COMMITTER_EMAIL", "ori@local"},
    };

    auto cleanup = [&]() { std::error_code e; fs::remove(tmp_index, e); };

    ProcResult head = runGit(repo.root, {"rev-parse", "-q", "--verify", "HEAD^{commit}"});
    std::string parent = head.ok() ? trimEnd(head.out) : std::string();

    if (!parent.empty() && !runGit(repo.root, {"read-tree", parent}, env).ok()) {
        cleanup();
        return false;
    }
    if (!runGit(repo.root, {"add", "-A"}, env).ok()) {
        cleanup();
        return false;
    }
    ProcResult tree = runGit(repo.root, {"write-tree"}, env);
    if (!tree.ok() || trimEnd(tree.out).empty()) {
        cleanup();
        return false;
    }

    std::vector<std::string> commit_args = {"commit-tree", trimEnd(tree.out)};
    if (!parent.empty()) {
        commit_args.push_back("-p");
        commit_args.push_back(parent);
    }
    commit_args.push_back("-m");
    commit_args.push_back("ori-backup: pre-edit snapshot");
    ProcResult commit = runGit(repo.root, commit_args, env);
    cleanup();
    std::string hash = trimEnd(commit.out);
    if (!commit.ok() || hash.empty()) {
        return false;
    }

    const std::string ref = "refs/ori/backups/" + backup_session_id + "/" + std::to_string(++backup_counter);
    if (!runGit(repo.root, {"update-ref", ref, hash}).ok()) {
        return false;
    }

    out_commit_hash = hash;
    if (out_ref) *out_ref = ref;
    if (out_root) *out_root = repo.root;
    return true;
}

void OriAssistant::deleteBackupRef(const std::string& repo_root, const std::string& ref) {
    if (repo_root.empty() || ref.empty()) return;
    runGit(repo_root, {"update-ref", "-d", ref});
}

// Snapshots older than a week belong to sessions that no longer exist (e.g. killed
// terminals) and can never be undone again, so let git garbage-collect them.
void OriAssistant::pruneStaleBackupRefs(const std::string& repo_root) {
    ProcResult refs = runGit(repo_root, {"for-each-ref", "--format=%(refname) %(committerdate:unix)", "refs/ori/backups/"});
    if (!refs.ok()) return;
    const long long cutoff = static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()) - 7LL * 24 * 3600;
    std::istringstream iss(refs.out);
    std::string line;
    while (std::getline(iss, line)) {
        size_t sp = line.rfind(' ');
        if (sp == std::string::npos) continue;
        std::string name = line.substr(0, sp);
        long long when = 0;
        try { when = std::stoll(line.substr(sp + 1)); } catch (...) { continue; }
        if (when < cutoff) {
            runGit(repo_root, {"update-ref", "-d", name});
        }
    }
}

bool OriAssistant::isProjectInitialized() const {
    RepoInfo repo;
    return probeBackup(repo) == BackupBlock::None;
}

bool OriAssistant::initProject(bool auto_confirm) {
    if (!gitAvailable()) {
        std::cout << RED << "[!] git was not found in PATH. Install git first, then run /init again." << RESET << std::endl;
        return false;
    }

    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    if (ec) {
        std::cout << RED << "[!] Could not determine the current directory." << RESET << std::endl;
        return false;
    }

    RepoInfo repo;
    if (locateRepo(repo)) {
        if (isUnsafeRoot(repo.root)) {
            std::cout << RED << "[!] Refusing to initialize: the git repository here is rooted at '" << repo.root
                      << "' (your home directory or '/'). cd into a project directory and try again." << RESET << std::endl;
            return false;
        }
    } else {
        if (isUnsafeRoot(cwd.string())) {
            std::cout << RED << "[!] Refusing to initialize your home directory or '/' as an Ori project. "
                      << "cd into a project directory and try again." << RESET << std::endl;
            return false;
        }
        bool go_ahead = auto_confirm;
        if (!go_ahead) {
            if (!isatty(STDIN_FILENO)) {
                std::cout << RED << "[!] '" << cwd.string() << "' is not a git repository. Re-run with -y to create one non-interactively."
                          << RESET << std::endl;
                return false;
            }
            std::cout << YELLOW << "'" << cwd.string() << "' is not a git repository. Create one here? (y/n): " << RESET;
            std::string answer;
            std::getline(std::cin, answer);
            if (std::cin.fail()) {
                std::cin.clear();
                answer = "n";
            }
            go_ahead = (answer == "y" || answer == "Y");
        }
        if (!go_ahead) {
            std::cout << YELLOW << "Initialization cancelled." << RESET << std::endl;
            return false;
        }
        if (!runGit(cwd.string(), {"init"}).ok()) {
            std::cout << RED << "[!] `git init` failed in " << cwd.string() << RESET << std::endl;
            return false;
        }
        if (!locateRepo(repo)) {
            std::cout << RED << "[!] Could not locate the newly created repository." << RESET << std::endl;
            return false;
        }
        std::cout << GREEN << "[✓] Created a new git repository in " << repo.root << RESET << std::endl;
    }

    fs::path marker = projectMarkerPath(repo);
    if (fs::exists(marker, ec)) {
        std::cout << GREEN << "[✓] " << repo.root << " is already an Ori project." << RESET << std::endl;
    } else {
        fs::create_directories(marker.parent_path(), ec);
        Json::Value info;
        info["ori_version"] = ORI_VERSION;
        info["root"] = repo.root;
        info["initialized_at"] = static_cast<Json::Int64>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        std::ofstream out(marker);
        if (!out.is_open()) {
            std::cout << RED << "[!] Could not write " << marker.string() << RESET << std::endl;
            return false;
        }
        Json::StreamWriterBuilder writer;
        out << Json::writeString(writer, info) << "\n";
        out.close();
        std::cout << GREEN << "[✓] Initialized Ori project at " << repo.root << RESET << std::endl;
    }

    if (config.git_backup_enabled) {
        std::cout << "    Pre-edit git snapshots and /undo are active for this project." << std::endl;
    } else {
        std::cout << YELLOW << "    Git backups are currently switched off; run /gitbackup on to use them." << RESET << std::endl;
    }
    return true;
}

// Takes a snapshot for the response being handled, once per response. Skips it
// (recording has_git_commit = false) unless the project was initialized with /init.
void OriAssistant::beginUndoSnapshot() {
    size_t hist_target = conversation_history.size() >= 2 ? conversation_history.size() - 2 : conversation_history.size();
    if (!undo_snapshots.empty() && undo_snapshots.back().history_size == hist_target) {
        return;
    }
    UndoSnapshot snap;
    snap.history_size = hist_target;
    snap.has_git_commit = gitBackupCommit(snap.git_commit_hash, &snap.backup_ref, &snap.repo_root);
    undo_snapshots.push_back(snap);
}

// Describes a path Ori is about to modify. Must be called BEFORE the change so that
// existed_before reflects the pre-edit state.
UndoPath OriAssistant::makeUndoPath(const std::string& path) const {
    UndoPath up;
    std::error_code ec;
    fs::path abs = fs::weakly_canonical(fs::absolute(path, ec), ec);
    if (ec) {
        up.abs_path = path;
        up.in_repo = false;
        return up;
    }
    up.abs_path = abs.string();
    up.existed_before = fs::exists(abs, ec);

    up.in_repo = false;
    if (!undo_snapshots.empty() && !undo_snapshots.back().repo_root.empty()) {
        fs::path root = fs::weakly_canonical(undo_snapshots.back().repo_root, ec);
        if (!ec) {
            fs::path rel = abs.lexically_relative(root);
            if (!rel.empty() && *rel.begin() != "..") {
                up.rel_path = rel.generic_string();
                up.in_repo = true;
            }
        }
    }
    return up;
}

// Registers a path as changed by Ori. The first record for a path wins because it
// holds the true pre-edit state.
void OriAssistant::addUndoPath(const UndoPath& p) {
    if (undo_snapshots.empty()) return;
    UndoSnapshot& snap = undo_snapshots.back();
    if (!snap.has_git_commit) return;
    for (const auto& existing : snap.paths) {
        if (existing.abs_path == p.abs_path) return;
    }
    snap.paths.push_back(p);
}

bool OriAssistant::performUndo() {
    if (undo_snapshots.empty()) {
        std::cout << YELLOW << "[!] No actions to undo." << RESET << std::endl;
        return false;
    }

    UndoSnapshot snap = undo_snapshots.back();
    undo_snapshots.pop_back();

    int restored = 0, removed = 0, skipped = 0, failed = 0;
    if (snap.has_git_commit && !snap.git_commit_hash.empty()) {
        // Restore only the paths Ori touched, newest change first. Everything else in
        // the work tree (the user's own uncommitted work) is left exactly as it is.
        for (auto it = snap.paths.rbegin(); it != snap.paths.rend(); ++it) {
            const UndoPath& p = *it;
            if (!p.in_repo) { skipped++; continue; }

            std::error_code ec;
            if (!p.existed_before) {
                // Ori created this file, so undoing means deleting it.
                fs::file_status st = fs::symlink_status(p.abs_path, ec);
                if (!ec && (fs::is_regular_file(st) || fs::is_symlink(st))) {
                    if (fs::remove(p.abs_path, ec) && !ec) removed++; else failed++;
                }
                continue;
            }

            const std::string spec = snap.git_commit_hash + ":" + p.rel_path;
            if (!runGit(snap.repo_root, {"cat-file", "-e", spec}).ok()) {
                // Not captured by the snapshot (e.g. a git-ignored file), so it cannot be restored.
                skipped++;
                continue;
            }
            ProcResult r = runGit(snap.repo_root, {"restore", "--source=" + snap.git_commit_hash, "--worktree", "--", p.rel_path});
            if (!r.ok()) {
                // git < 2.23 has no `restore`: fall back to checkout and un-stage the path again.
                r = runGit(snap.repo_root, {"checkout", snap.git_commit_hash, "--", p.rel_path});
                if (r.ok()) runGit(snap.repo_root, {"reset", "-q", "--", p.rel_path});
            }
            if (r.ok()) restored++; else failed++;
        }
        deleteBackupRef(snap.repo_root, snap.backup_ref);
    }

    if (conversation_history.size() > snap.history_size) {
        conversation_history.resize(snap.history_size);
    }

    std::cout << GREEN << "[✓] Undo performed successfully! " << RESET;
    if (snap.has_git_commit) {
        std::cout << GREEN << "Restored " << restored << " file(s)";
        if (removed > 0) std::cout << ", removed " << removed << " created file(s)";
        std::cout << " changed by Ori; your other uncommitted work was left untouched. " << RESET;
        if (skipped > 0 || failed > 0) {
            std::cout << YELLOW << "\n[!] " << skipped << " path(s) could not be restored automatically (outside the repository, git-ignored or not in the snapshot)"
                      << (failed > 0 ? " and " + std::to_string(failed) + " failed" : std::string()) << "." << RESET << "\n";
        }
    }
    std::cout << GREEN << "The conversation prompt/response was removed from history." << RESET << std::endl;
    if (!snap.has_git_commit) {
        std::cout << YELLOW << "[i] No git snapshot exists for that action, so files were not reverted. "
                  << "Run /init in a project directory to enable file restore." << RESET << std::endl;
    } else {
        std::cout << YELLOW << "[i] Effects of shell commands are not reverted." << RESET << std::endl;
    }
    return true;
}

std::string OriAssistant::sendQuery(const std::string& prompt) {
    if (!active_provider) {
        return colorize(RED, "Error: No active API provider is configured.");
    }

    // Per-request context (AGENTS.md rules, skills, retrieved RAG chunks). It is
    // attached to the outgoing request only. Persisting it in conversation_history
    // would re-send every earlier copy on every later turn, so token usage would
    // grow quadratically over a long session.
    std::string prefix_ctx = agentsManager.getAgentsPromptContext();
    prefix_ctx += skillsManager.getSkillsPromptContext();
    prefix_ctx += ragMemory.getPromptContext(prompt);

    std::string outgoing_prompt = prompt;
    if (!prefix_ctx.empty()) {
        outgoing_prompt = prefix_ctx + "\n" + prompt;
    }

    if (config.debug) {
        std::cerr << "[DEBUG] Active API Config / Model: " << config.active_api_config << "\n";
        std::cerr << "[DEBUG] Prompt: " << outgoing_prompt << "\n";
    }

    // The history keeps the bare prompt only.
    conversation_history.push_back({"user", prompt});

    // Build the request from the history, swapping in the context-enriched prompt for this turn.
    std::vector<std::pair<std::string, std::string>> provider_history;
    provider_history.reserve(conversation_history.size());
    for (const auto& msg : conversation_history) {
        provider_history.push_back({msg.role, msg.content});
    }
    provider_history.back().second = outgoing_prompt;

    std::string response = active_provider->sendQuery(prompt, provider_history);

    conversation_history.push_back({"assistant", response});

    if (config.rag_memory_enabled) {
        ragMemory.addChunk("User: " + prompt + "\nAssistant: " + response, "conversation");
    }

    return response;
}

#ifdef CURL_FOUND
static bool curl_initialized = false;
#endif

OriAssistant::OriAssistant() {
#ifdef CURL_FOUND
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true;
    }
#endif
}

OriAssistant::~OriAssistant() {
    // Snapshots can not be undone after the session ends, so release their refs.
    for (const auto& snap : undo_snapshots) {
        if (snap.has_git_commit) {
            deleteBackupRef(snap.repo_root, snap.backup_ref);
        }
    }
#ifdef CURL_FOUND
    curl_global_cleanup();
#endif
}

bool OriAssistant::initialize() {
    std::signal(SIGINT, sigint_handler);
    // Create config directory if it doesn't exist
    const char* home_dir = std::getenv("HOME");
    std::string config_dir_path;
    if (home_dir != nullptr) {
        config_dir_path = std::string(home_dir) + "/.config/ori";
        struct stat st;
        if (stat(config_dir_path.c_str(), &st) == -1) {
            mkdir(config_dir_path.c_str(), 0755);
        }
    }
    
    configManager.loadConfig(config);

    // Load providers from keys.json
    std::string keys_path = config_dir_path + "/keys.json";
    std::ifstream keys_file(keys_path);
    if (!keys_file.is_open()) {
        std::cerr << RED << "Error: Could not open " << keys_path << ". Please run the initial setup." << RESET << std::endl;
        return false;
    }

    Json::Value keys_json;
    keys_file >> keys_json;

    for (const auto& key_entry : keys_json) {
        std::string id = key_entry.get("id", "").asString();
        std::string provider_name = key_entry.get("provider", "").asString();
        std::string api_key = key_entry.get("api_key", "").asString();
        std::string model = key_entry.get("model", "").asString();

        if (id.empty() || provider_name.empty() || api_key.empty()) {
            continue;
        }

        std::unique_ptr<APIProvider> provider;
        if (provider_name == "openrouter") {
            provider = std::make_unique<OpenRouterProvider>();
        } else if (provider_name == "google") {
            provider = std::make_unique<GoogleProvider>();
        } else if (provider_name == "huggingface") {
            provider = std::make_unique<HuggingFaceProvider>();
        } else {
            continue;
        }

        provider->setApiKey(api_key);
        provider->setModel(model);
        
        ProviderInfo p_info;
        p_info.provider = std::move(provider);
        p_info.details = key_entry; // Store the full JSON entry
        providers_info[id] = std::move(p_info);
    }

    if (providers_info.empty()) {
        std::cerr << RED << "Error: No valid API providers found in " << keys_path << "." << RESET << std::endl;
        return false;
    }

    agentsManager.discoverAndLoad();
    skillsManager.discoverAndLoad(config.skills_memory_enabled);
    ragMemory.setEnabled(config.rag_memory_enabled);
    ragMemory.load();

    // Set active provider
    auto it = providers_info.find(config.active_api_config);
    if (it != providers_info.end()) {
        active_provider = it->second.provider.get();
    } else {
        // Search if any provider entry has this model name
        bool found_model = false;
        for (auto& pair : providers_info) {
            if (pair.second.details.isMember("model") && pair.second.details["model"].asString() == config.active_api_config) {
                active_provider = pair.second.provider.get();
                found_model = true;
                break;
            }
        }
        if (!found_model) {
            // Fallback to the first available provider using its own configured model & ID
            auto first_it = providers_info.begin();
            active_provider = first_it->second.provider.get();
            config.active_api_config = first_it->first;
            configManager.saveConfig(config);
        }
    }
    
    return true;
}

void OriAssistant::run() {
    if (config.debug) {
        std::cerr << "Debug: Before checkForUpdates" << std::endl;
    }
    checkForUpdates(true);
    if (config.debug) {
        std::cerr << "Debug: After checkForUpdates" << std::endl;
    }
    if (!config.no_clear) {
        // Clear screen before showing banner
        std::system("clear");
    }
    
    // Save cursor position for future reference
    printf("\033[s");
    
    showBanner();
    
    // Save cursor position after banner (for potential future use)
    printf("\033[s");
    
    while (true) {
        std::string input = readInput();
        
        if (std::cin.fail() || std::cin.eof()) {
            break;
        }
        
        if (input.rfind('/', 0) == 0) {
            if (input == "/quit" || input == "/exit") {
                break;
            } else if (input == "/help") {
                showHelp();
            } else if (input == "/clear") {
                std::system("clear");
            } else if (input.rfind("/cat ", 0) == 0) {
                std::string file_path = input.substr(5);
                std::ifstream file(file_path);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    file.close();
                    std::cout << content << std::endl;
                    pre_prompt_context += "The user has read the file '" + file_path + "' with the following content:\n---\n" + content + "\n---";
                } else {
                    std::cout << RED << "Error: could not open file " << file_path << RESET << std::endl;
                }
            } else if (input.rfind("/exec ", 0) == 0) {
                std::string command = input.substr(6);
                handleCommandExecution(command, false, false); // Manual confirm for /exec command
            } else if (input.rfind("/autoexec ", 0) == 0) {
                std::string mode = input.substr(10);
                if (mode == "ask" || mode == "yes" || mode == "no") {
                    config.auto_execute_commands_mode = mode;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "Auto-execute mode set to: " << mode << RESET << std::endl;
                } else {
                    std::cout << RED << "Invalid autoexec mode. Use 'ask', 'yes', or 'no'." << RESET << std::endl;
                }
            } else if (input.rfind("/model ", 0) == 0) {
                std::string api_config_id = input.substr(7);
                auto it = providers_info.find(api_config_id);
                if (it != providers_info.end()) {
                    active_provider = it->second.provider.get();
                    config.active_api_config = api_config_id;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "Active model set to: " << api_config_id << RESET << std::endl;
                } else {
                    std::cout << RED << "Model '" << api_config_id << "' not found. Available models:" << RESET << std::endl;
                    for (const auto& pair : providers_info) {
                        std::cout << "  - " << pair.first << (pair.second.details.isMember("model") ? " (" + pair.second.details["model"].asString() + ")" : "") << std::endl;
                    }
                }
            } else if (input == "/thinking") {
                // Check if a 'thinking' role model is configured
                bool found_thinking_model = false;
                for (const auto& pair : providers_info) {
                    if (pair.second.details.isMember("role") && pair.second.details["role"].asString() == "thinking") {
                        active_provider = pair.second.provider.get();
                        config.active_api_config = pair.first;
                        configManager.saveConfig(config);
                        std::cout << YELLOW << "Thinking mode activated using model: " << pair.first << RESET << std::endl;
                        found_thinking_model = true;
                        break;
                    }
                }
                if (!found_thinking_model) {
                    std::cout << YELLOW << "No 'thinking' role model configured in keys.json." << RESET << std::endl;
                }
            } else if (input == "/agents" || input == "/agent") {
                agentsManager.discoverAndLoad();
                if (agentsManager.hasRules()) {
                    std::cout << GREEN << "[✓] Discovered AGENTS.md rules:" << RESET << std::endl;
                    for (const auto& rule : agentsManager.getLoadedRules()) {
                        std::cout << BOLD << CYAN << "  [+] " << rule.filepath << RESET << std::endl;
                        std::cout << rule.content << std::endl;
                    }
                } else {
                    std::cout << YELLOW << "[!] No AGENTS.md instructions found in workspace hierarchy." << RESET << std::endl;
                }
            } else if (input.rfind("/task", 0) == 0) {
                std::string arg = input.length() > 5 ? input.substr(6) : "";
                if (arg == "list" || arg.empty()) {
                    taskDispatcher.displayTasks();
                } else if (arg == "clear") {
                    taskDispatcher.clearTasks();
                    std::cout << GREEN << "[✓] Task queue cleared." << RESET << std::endl;
                } else if (arg == "run") {
                    taskDispatcher.runAllTasks(*this);
                } else if (arg == "next") {
                    taskDispatcher.runNextTask(*this);
                } else if (arg.rfind("decompose ", 0) == 0) {
                    std::string goal = arg.substr(10);
                    std::cout << CYAN << "[➜] Decomposing goal into subtasks..." << RESET << std::endl;
                    taskDispatcher.decomposeGoal(*this, goal);
                    taskDispatcher.displayTasks();
                } else if (arg.rfind("create ", 0) == 0) {
                    std::string task_spec = arg.substr(7);
                    size_t pipe_pos = task_spec.find('|');
                    std::string title = task_spec;
                    std::string desc = task_spec;
                    std::string role = "generalist";
                    if (pipe_pos != std::string::npos) {
                        title = task_spec.substr(0, pipe_pos);
                        desc = task_spec.substr(pipe_pos + 1);
                    }
                    int tid = taskDispatcher.addTask(title, desc, role);
                    std::cout << GREEN << "[✓] Added Task #" << tid << ": " << title << RESET << std::endl;
                } else {
                    std::cout << YELLOW << "Task Dispatcher Commands:" << RESET << std::endl;
                    std::cout << "  /task list               - List task queue" << std::endl;
                    std::cout << "  /task decompose <goal>  - Auto-decompose a goal into subtasks" << std::endl;
                    std::cout << "  /task create <title>|<desc> - Manually add a task" << std::endl;
                    std::cout << "  /task run                - Run all tasks" << std::endl;
                    std::cout << "  /task next               - Run next task" << std::endl;
                    std::cout << "  /task clear              - Clear task queue" << std::endl;
                }
            } else if (input.rfind("/subagent", 0) == 0) {
                std::cout << BOLD << "--- Registered Subagents ---" << RESET << std::endl;
                std::cout << "  [+] generalist - General reasoning and problem solving" << std::endl;
                std::cout << "  [+] coder      - Software engineering and code refactoring" << std::endl;
                std::cout << "  [+] reviewer   - Code review and bug inspection" << std::endl;
                std::cout << "  [+] executor   - Linux terminal and command execution" << std::endl;
                std::cout << "  [+] planner    - Task planning and goal decomposition" << std::endl;
                std::cout << BOLD << "----------------------------" << RESET << std::endl;
            } else if (input == "/undo") {
                performUndo();
            } else if (input == "/init") {
                initProject(false);
            } else if (input.rfind("/gitbackup", 0) == 0) {
                std::string arg = input.length() > 10 ? input.substr(11) : "";
                if (arg == "on" || arg == "true" || arg == "yes") {
                    config.git_backup_enabled = true;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] Pre-edit git backup commits enabled." << RESET << std::endl;
                } else if (arg == "off" || arg == "false" || arg == "no") {
                    config.git_backup_enabled = false;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] Pre-edit git backup commits disabled." << RESET << std::endl;
                } else {
                    config.git_backup_enabled = !config.git_backup_enabled;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] Pre-edit git backup commits set to: " << (config.git_backup_enabled ? "ON" : "OFF") << RESET << std::endl;
                }
                if (config.git_backup_enabled && !isProjectInitialized()) {
                    std::cout << YELLOW << "[i] Snapshots only run in projects initialized with /init; this directory is not one yet." << RESET << std::endl;
                }
            } else if (input.rfind("/cmdoutput", 0) == 0) {
                std::string arg = input.length() > 10 ? input.substr(11) : "";
                if (arg == "on" || arg == "true" || arg == "yes") {
                    config.show_command_output = true;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] Command output display enabled." << RESET << std::endl;
                } else if (arg == "off" || arg == "false" || arg == "no") {
                    config.show_command_output = false;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] Command output display disabled." << RESET << std::endl;
                } else {
                    config.show_command_output = !config.show_command_output;
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] Command output display set to: " << (config.show_command_output ? "ON" : "OFF") << RESET << std::endl;
                }
            } else if (input.rfind("/skill", 0) == 0 || input.rfind("/skills", 0) == 0) {
                std::string arg = "";
                if (input.rfind("/skills", 0) == 0 && input.length() > 7) arg = input.substr(8);
                else if (input.rfind("/skill", 0) == 0 && input.length() > 6) arg = input.substr(7);

                if (arg == "list" || arg.empty()) {
                    skillsManager.discoverAndLoad(config.skills_memory_enabled);
                    std::cout << BOLD << "--- Configured & Learned Agent Skills ---" << RESET << std::endl;
                    for (const auto& skill : skillsManager.getSkills()) {
                        std::cout << BOLD << CYAN << "  [+] " << skill.name << RESET
                                  << " (" << (skill.is_builtin ? "built-in" : (skill.is_learned ? "learned" : "configured")) << ")\n"
                                  << "      " << skill.description << std::endl;
                    }
                    std::cout << BOLD << "---------------------------------------" << RESET << std::endl;
                } else if (arg.rfind("show ", 0) == 0) {
                    std::string name = arg.substr(5);
                    bool found = false;
                    for (const auto& s : skillsManager.getSkills()) {
                        if (s.name == name) {
                            std::cout << BOLD << "Skill: " << s.name << RESET << " (" << s.source_file << ")\n";
                            std::cout << "Description: " << s.description << "\n";
                            std::cout << "Instructions:\n" << s.instructions << std::endl;
                            found = true;
                            break;
                        }
                    }
                    if (!found) std::cout << RED << "Skill '" << name << "' not found." << RESET << std::endl;
                } else if (arg.rfind("learn ", 0) == 0) {
                    std::string spec = arg.substr(6);
                    size_t p1 = spec.find('|');
                    size_t p2 = p1 != std::string::npos ? spec.find('|', p1 + 1) : std::string::npos;
                    if (p1 != std::string::npos && p2 != std::string::npos) {
                        std::string name = spec.substr(0, p1);
                        std::string desc = spec.substr(p1 + 1, p2 - p1 - 1);
                        std::string inst = spec.substr(p2 + 1);
                        auto trim = [](std::string& s) {
                            size_t a = s.find_first_not_of(" \t");
                            if (a != std::string::npos) s = s.substr(a);
                            size_t b = s.find_last_not_of(" \t");
                            if (b != std::string::npos) s = s.substr(0, b + 1);
                        };
                        trim(name); trim(desc); trim(inst);
                        skillsManager.addLearnedSkill(name, desc, inst);
                        std::cout << GREEN << "[✓] Learned skill '" << name << "' saved to persistent memory." << RESET << std::endl;
                    } else {
                        std::cout << YELLOW << "Usage: /skill learn <name> | <description> | <instructions>" << RESET << std::endl;
                    }
                } else if (arg.rfind("forget ", 0) == 0) {
                    std::string name = arg.substr(7);
                    if (skillsManager.removeLearnedSkill(name)) {
                        std::cout << GREEN << "[✓] Learned skill '" << name << "' removed." << RESET << std::endl;
                    } else {
                        std::cout << RED << "Learned skill '" << name << "' not found." << RESET << std::endl;
                    }
                } else {
                    std::cout << YELLOW << "Skills System Commands:" << RESET << std::endl;
                    std::cout << "  /skills list                                - List all skills" << std::endl;
                    std::cout << "  /skill show <name>                          - Show skill details" << std::endl;
                    std::cout << "  /skill learn <name> | <desc> | <instruct>  - Learn a new skill" << std::endl;
                    std::cout << "  /skill forget <name>                        - Forget a learned skill" << std::endl;
                }
            } else if (input.rfind("/rag", 0) == 0) {
                std::string arg = input.length() > 4 ? input.substr(5) : "";
                if (arg == "on" || arg == "enable") {
                    config.rag_memory_enabled = true;
                    ragMemory.setEnabled(true);
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] RAG Memory enabled." << RESET << std::endl;
                } else if (arg == "off" || arg == "disable") {
                    config.rag_memory_enabled = false;
                    ragMemory.setEnabled(false);
                    configManager.saveConfig(config);
                    std::cout << GREEN << "[✓] RAG Memory disabled." << RESET << std::endl;
                } else if (arg.rfind("add ", 0) == 0) {
                    std::string text = arg.substr(4);
                    ragMemory.addChunk(text, "user_added");
                    std::cout << GREEN << "[✓] Added text chunk to RAG Memory." << RESET << std::endl;
                } else if (arg.rfind("search ", 0) == 0) {
                    std::string q = arg.substr(7);
                    ragMemory.setEnabled(true);
                    auto results = ragMemory.retrieveRelevant(q, 5);
                    std::cout << BOLD << "--- RAG Memory Search Results ---" << RESET << std::endl;
                    if (results.empty()) {
                        std::cout << "No matching chunks found." << std::endl;
                    } else {
                        for (const auto& chunk : results) {
                            std::cout << CYAN << "[Chunk " << chunk.id << "] (" << chunk.source << ")" << RESET << "\n" << chunk.content << std::endl;
                        }
                    }
                    std::cout << BOLD << "--------------------------------" << RESET << std::endl;
                } else if (arg == "clear") {
                    ragMemory.clear();
                    std::cout << GREEN << "[✓] RAG Memory cleared." << RESET << std::endl;
                } else {
                    std::cout << BOLD << "RAG Memory Status: " << (config.rag_memory_enabled ? GREEN + "ENABLED" : YELLOW + "DISABLED") << RESET << "\n";
                    std::cout << "Total Chunks Indexed: " << ragMemory.getChunks().size() << " (max " << ragMemory.getMaxChunks() << ", oldest evicted first)\n\n";
                    std::cout << YELLOW << "RAG Memory Commands:" << RESET << std::endl;
                    std::cout << "  /rag on / off             - Toggle RAG Memory" << std::endl;
                    std::cout << "  /rag add <text>           - Add text chunk to RAG Memory" << std::endl;
                    std::cout << "  /rag search <query>       - Search RAG Memory" << std::endl;
                    std::cout << "  /rag clear                - Clear all RAG Memory chunks" << std::endl;
                }
            } else {
                std::cout << RED << "Unknown command: " << input << RESET << std::endl;
            }
        } else if (!input.empty()) {
            processSingleRequest(input, false); // Interactive mode, no auto-confirm
        }
    }
}

void OriAssistant::showBanner() {
    if (!config.no_banner) {
        // Display banner
        std::cout << BLUE << R"(
    ███████    ███████████   █████            ███████████ █████  █████ █████
  ███▒▒▒▒▒███ ▒▒███▒▒▒▒▒███ ▒▒███            ▒█▒▒▒███▒▒▒█▒▒███  ▒▒███ ▒▒███ 
 ███     ▒▒███ ▒███    ▒███  ▒███            ▒   ▒███  ▒  ▒███   ▒███  ▒███ 
▒███      ▒███ ▒██████████   ▒███  ██████████    ▒███     ▒███   ▒███  ▒███ 
▒███      ▒███ ▒███▒▒▒▒▒███  ▒███ ▒▒▒▒▒▒▒▒▒▒     ▒███     ▒███   ▒███  ▒███ 
▒▒███     ███  ▒███    ▒███  ▒███                ▒███     ▒███   ▒███  ▒███ 
 ▒▒▒███████▒   █████   █████ █████               █████    ▒▒████████   █████
   ▒▒▒▒▒▒▒    ▒▒▒▒▒   ▒▒▒▒▒ ▒▒▒▒▒               ▒▒▒▒▒      ▒▒▒▒▒▒▒▒   ▒▒▒▒▒
)" << RESET << std::endl;
        std::cout << BOLD << BLUE << "ORI Terminal Assistant v" << ORI_VERSION << RESET << "\n";
        // Single newline after instructions to avoid empty-space gap
        std::cout << "Type '/help' for available commands or '/quit' to exit.\n";
    }
}

void OriAssistant::displayCommandLog() {
    std::cout << BOLD << "--- Command Execution Log ---" << RESET << std::endl;
    if (command_log.empty()) {
        std::cout << "No commands executed yet." << std::endl;
    } else {
        for (const auto& entry : command_log) {
            std::cout << "> " << BOLD << CYAN << entry.command << RESET << std::endl;
            std::cout << entry.output << std::endl;
        }
    }
    std::cout << BOLD << "---------------------------" << RESET << std::endl;
}

void OriAssistant::handleResponse(const std::string& response, bool auto_confirm) {
    // Move to a new line to ensure clean output
    std::cout << "\n";

    size_t current_pos = 0;
    while (true) {
        // Find next tag: [exec], [edit], [writefile], or [learn_skill]
        size_t exec_start = response.find("[exec]", current_pos);
        size_t exec_end = (exec_start != std::string::npos) ? response.find("[/exec]", exec_start) : std::string::npos;
        size_t edit_start = response.find("[edit]", current_pos);
        size_t edit_end = (edit_start != std::string::npos) ? response.find("[/edit]", edit_start) : std::string::npos;
        size_t writefile_start = response.find("[writefile(", current_pos);
        size_t writefile_end = (writefile_start != std::string::npos) ? response.find("[/writefile]", writefile_start) : std::string::npos;
        size_t learn_start = response.find("[learn_skill]", current_pos);
        size_t learn_end = (learn_start != std::string::npos) ? response.find("[/learn_skill]", learn_start) : std::string::npos;

        // Determine which tag comes next
        size_t next_pos = std::string::npos;
        enum TagType { NONE, EXEC, EDIT, WRITEFILE, LEARN_SKILL } next_tag = NONE;
        if (exec_start != std::string::npos && (next_pos == std::string::npos || exec_start < next_pos)) {
            next_pos = exec_start; next_tag = EXEC;
        }
        if (edit_start != std::string::npos && (next_pos == std::string::npos || edit_start < next_pos)) {
            next_pos = edit_start; next_tag = EDIT;
        }
        if (writefile_start != std::string::npos && (next_pos == std::string::npos || writefile_start < next_pos)) {
            next_pos = writefile_start; next_tag = WRITEFILE;
        }
        if (learn_start != std::string::npos && (next_pos == std::string::npos || learn_start < next_pos)) {
            next_pos = learn_start; next_tag = LEARN_SKILL;
        }


        if (next_tag == NONE) {
            // Print remaining
            if (current_pos < response.length()) {
                std::string remaining = response.substr(current_pos);
                std::istringstream iss(remaining);
                std::string line;
                while (std::getline(iss, line)) {
                    if (!line.empty()) {
                        line.erase(0, line.find_first_not_of(" \t"));
                        std::cout << line << "\n";
                    }
                }
                std::cout.flush();
            }
            break;
        }

        // Print any text before the tag
        if (next_pos > current_pos) {
            std::cout << response.substr(current_pos, next_pos - current_pos);
        }

        if (next_tag == EXEC) {
            // Handle exec block
            if (exec_end == std::string::npos) break; // malformed
            size_t cmd_start = exec_start + strlen("[exec]");
            std::string command = response.substr(cmd_start, exec_end - cmd_start);
            handleCommandExecution(command, auto_confirm);
            current_pos = exec_end + strlen("[/exec]");
            continue;
        } else if (next_tag == EDIT) {
            // Take the pre-edit snapshot (only inside an initialized Ori project)
            beginUndoSnapshot();

            // Handle edit block using strict JSON parsing (JsonCpp)
            if (edit_end == std::string::npos) break; // malformed
            size_t json_start = edit_start + strlen("[edit]");
            std::string payload = response.substr(json_start, edit_end - json_start);
            // Trim whitespace
            auto trim = [](std::string &s) {
                size_t a = s.find_first_not_of(" \t\n\r");
                if (a == std::string::npos) { s.clear(); return; }
                size_t b = s.find_last_not_of(" \t\n\r");
                s = s.substr(a, b - a + 1);
            };
            trim(payload);

            Json::CharReaderBuilder readerBuilder;
            std::string errs;
            Json::Value root;
            std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
            bool parsed = false;
            if (!payload.empty()) {
                parsed = reader->parse(payload.c_str(), payload.c_str() + payload.size(), &root, &errs);
            }

            if (!parsed) {
                std::cout << YELLOW << "[edit] payload is not valid JSON. Assistant must return strictly escaped JSON inside [edit] tags." << RESET << std::endl;
                if (!errs.empty()) std::cerr << "[ORI_DEBUG] json parse errors: " << errs << std::endl;
                std::cout << payload << std::endl;
                current_pos = edit_end + strlen("[/edit]");
                continue;
            }

            std::string operation = root.get("operation", "").asString();
            if (operation.empty()) {
                std::cout << YELLOW << "[edit] block missing 'operation' field" << RESET << std::endl;
                current_pos = edit_end + strlen("[/edit]");
                continue;
            }

            if (operation == "compare") {
                if (root.isMember("files") && root["files"].isArray() && root["files"].size() >= 2) {
                    std::string f1 = root["files"][0].asString();
                    std::string f2 = root["files"][1].asString();
                    OriEdit::showDiff(f1, f2);
                } else {
                    std::cout << YELLOW << "[edit] compare requires a 'files' array with at least two file paths" << RESET << std::endl;
                }
            } else if (operation == "replace" || operation == "modify" || operation == "create") {
                std::string filename = root.get("file", "").asString();
                std::string newcontent;
                if (root.isMember("content")) {
                    if (root["content"].isObject() && root["content"].isMember("new")) {
                        newcontent = root["content"]["new"].asString();
                    } else if (root["content"].isString()) {
                        newcontent = root["content"].asString();
                    }
                } else if (root.isMember("new")) {
                    newcontent = root["new"].asString();
                }

                if (filename.empty()) {
                    std::cout << YELLOW << "[edit] missing 'file' field" << RESET << std::endl;
                } else {
                    EditOperation op;
                    op.type = operation;
                    op.filename = filename;
                    op.newContent = newcontent;
                    op.preview = false;
                    op.diff = false;
                    // For create operations, don't attempt to backup the non-existent file
                    op.backup = (operation != "create");
                    op.interactive = false;
                    op.safe = true;

                    if (op.newContent.empty()) {
                        std::cout << YELLOW << "[edit] no new content found in JSON payload for file " << filename << RESET << std::endl;
                    } else {
                        UndoPath undo_path = makeUndoPath(filename); // capture pre-edit state
                        if (OriEdit::applyChanges(op)) {
                            addUndoPath(undo_path);
                        }
                    }
                }
            } else if (operation == "rename") {
                std::string filename = root.get("file", "").asString();
                std::string newname = root.get("newname", "").asString();
                if (filename.empty() || newname.empty()) {
                    std::cout << YELLOW << "[edit] rename requires 'file' and 'newname' fields" << RESET << std::endl;
                } else {
                    UndoPath undo_from = makeUndoPath(filename);
                    UndoPath undo_to = makeUndoPath(newname);
                    if (std::rename(filename.c_str(), newname.c_str()) == 0) {
                        addUndoPath(undo_from);
                        addUndoPath(undo_to);
                        std::cout << GREEN << "Renamed " << filename << " -> " << newname << RESET << std::endl;
                    } else {
                        std::cout << RED << "Failed to rename " << filename << RESET << std::endl;
                    }
                }
            } else {
                std::cout << YELLOW << "[edit] unsupported operation: " << operation << RESET << std::endl;
            }

            current_pos = edit_end + strlen("[/edit]");
            continue;
        } else if (next_tag == WRITEFILE) {
            // Take the pre-edit snapshot (only inside an initialized Ori project)
            beginUndoSnapshot();

            // Handle writefile block
            if (writefile_end == std::string::npos) break; // malformed
            size_t fn_start = writefile_start + strlen("[writefile(");
            size_t fn_end = response.find(")]", fn_start);
            if (fn_end == std::string::npos) break; // malformed
            std::string filename = response.substr(fn_start, fn_end - fn_start);
            size_t content_start = fn_end + strlen(")]");
            std::string content = response.substr(content_start, writefile_end - content_start);

            // Create directories if they don't exist
            size_t last_slash = filename.find_last_of("/");
            if (last_slash != std::string::npos) {
                std::string dir = filename.substr(0, last_slash);
                std::filesystem::create_directories(dir);
            }

            UndoPath undo_path = makeUndoPath(filename); // capture pre-write state
            std::ofstream file(filename);
            if (file.is_open()) {
                addUndoPath(undo_path);
                file << content;
                file.close();
                std::cout << GREEN << "File created: " << filename << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to create file: " << filename << RESET << std::endl;
            }
            current_pos = writefile_end + strlen("[/writefile]");
            continue;
        } else if (next_tag == LEARN_SKILL) {
            if (learn_end == std::string::npos) break; // malformed
            size_t payload_start = learn_start + strlen("[learn_skill]");
            std::string payload = response.substr(payload_start, learn_end - payload_start);

            Json::CharReaderBuilder readerBuilder;
            std::string errs;
            Json::Value root;
            std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
            if (reader->parse(payload.c_str(), payload.c_str() + payload.size(), &root, &errs)) {
                std::string name = root.get("name", "").asString();
                std::string desc = root.get("description", "").asString();
                std::string inst = root.get("instructions", "").asString();
                if (!name.empty()) {
                    skillsManager.addLearnedSkill(name, desc, inst);
                    std::cout << GREEN << "[✓] Automatically learned new skill: " << name << RESET << std::endl;
                }
            }
            current_pos = learn_end + strlen("[/learn_skill]");
            continue;
        }
    }
}

void OriAssistant::processSingleRequest(const std::string& prompt, bool auto_confirm) {
    std::string full_prompt = prompt;
    if (!pre_prompt_context.empty()) {
        full_prompt = pre_prompt_context + "\n" + prompt;
        pre_prompt_context.clear();
    }
    
    // Get response and handle it
    handleResponse(sendQuery(full_prompt), auto_confirm);
}

pid_t popen2(const char *command, int *read_fd) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }

    if (pid == 0) { // child
        close(pipe_fd[0]); // close read end
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO); // also redirect stderr
        close(pipe_fd[1]);
        setpgid(0, 0); // create new process group
        execl("/bin/sh", "sh", "-c", command, NULL);
        _exit(127); // if execl fails
    }

    // parent
    close(pipe_fd[1]); // close write end
    *read_fd = pipe_fd[0];
    return pid;
}

void OriAssistant::handleCommandExecution(const std::string& raw_command, bool auto_confirm, bool send_to_ai) {
    std::string command = prepare_elevated_command(raw_command);
    bool confirmed = false;
    if (auto_confirm) {
        confirmed = true;
    } else {
        if (config.auto_execute_commands_mode == "yes") {
            confirmed = true;
            std::cout << YELLOW << "Auto-confirming command execution: " << BOLD << CYAN << "<< " << command << " >> " << RESET << "\n";
        } else if (config.auto_execute_commands_mode == "no") {
            confirmed = false;
            std::cout << YELLOW << "Auto-declining command execution: " << BOLD << CYAN << "<< " << command << " >> " << RESET << "\n";
        } else { // "ask" or any other value
            // Warn if elevated privilege commands present but still ask for interactive confirmation
            if (command.find("sudo") != std::string::npos || command.find("pkexec") != std::string::npos || command.find(" su ") != std::string::npos) {
                std::cout << YELLOW << "[!] Warning: this command requests elevated privileges (contains 'pkexec'/'sudo'/'su'). It may prompt for a password when run." << RESET << std::endl;
            }

            std::cout << YELLOW << "Execute the following command? (y/n): " << RESET << BOLD << CYAN << "<< " << command << " >> " << RESET;
            std::string confirmation;
            interrupted_flag = false;
            std::getline(std::cin, confirmation);
            if (std::cin.fail() || interrupted_flag) {
                std::cin.clear();
                interrupted_flag = false;
                confirmation = "n";
                std::cout << std::endl;
            }

            if (confirmation == "y" || confirmation == "Y") {
                confirmed = true;
            }
        }
    }

    if (confirmed) {
        int read_fd;
        pid_t pid = popen2(command.c_str(), &read_fd);
        if (pid == -1) {
            command_log.push_back({command, "Failed to execute command."});
            return;
        }

        std::string result;
        char buffer[256];
        ssize_t bytes_read;

        fcntl(read_fd, F_SETFL, O_NONBLOCK);

        if (!show_command_log) {
            keep_running = true;
            interrupted_flag = false;
            std::thread spinner_thread;
            if (g_is_interactive_mode && !g_is_gui_mode) {
                spinner_thread = std::thread(run_spinner, "executing command...");
            } else if (!g_is_gui_mode) {
                std::cout << colorize(CYAN, "[➜] Executing command: ") << command << "..." << std::endl;
            }

            while (true) {
                if (interrupted_flag) {
                    kill(-pid, SIGKILL);
                    waitpid(pid, NULL, 0);
                    result += "\n[Command cancelled by user]";
                    sendQuery("User cancelled the command execution.");
                    break;
                }

                bytes_read = read(read_fd, buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    result += buffer;
                }

                int status;
                pid_t wait_result = waitpid(pid, &status, WNOHANG);
                if (wait_result == pid) {
                    // Drain remaining output
                    while ((bytes_read = read(read_fd, buffer, sizeof(buffer) - 1)) > 0) {
                        buffer[bytes_read] = '\0';
                        result += buffer;
                    }
                    break;
                }
                if (wait_result == -1) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            keep_running = false;
            if (spinner_thread.joinable()) {
                spinner_thread.join();
            }
        } else {
            // blocking read when log is shown
            while ((bytes_read = read(read_fd, buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytes_read] = '\0';
                result += buffer;
            }
            waitpid(pid, NULL, 0);
        }
        
        interrupted_flag = false;
        close(read_fd);

        command_log.push_back({command, result});

        if (config.show_command_output) {
            std::cout << colorize(CYAN, "\n+---------------------------- [ COMMAND OUTPUT ] ----------------------------+") << std::endl;
            if (!result.empty()) {
                std::cout << result;
                if (result.back() != '\n') std::cout << "\n";
            }
            std::cout << colorize(CYAN, "+-------------------------- [ END COMMAND OUTPUT ] --------------------------+\n") << std::endl;
        } else {
            std::cout << YELLOW << "[!] Command output printing is disabled. Type '/cmdoutput on' to enable." << RESET << std::endl;
        }

        if (send_to_ai) {
            std::string feedback_prompt = "The command \"" + command + "\" produced the following output:\n---\n" + result + "\n---\nPlease summarize this output or answer the original question based on it.";
            processSingleRequest(feedback_prompt, auto_confirm);
        } else {
            pre_prompt_context += "The user executed the command `" + command + "` with the following output:\n---\n" + result + "\n---";
        }
        } else {
            std::cout << YELLOW << "Command execution cancelled." << RESET << "\n\n";
            sendQuery("The user cancelled the command execution. Please inform the user that you cannot answer the question without running the command.");
    }
} // Closing brace for OriAssistant::handleCommandExecution

void OriAssistant::setExecutablePath(const std::string& path) {
    executable_path = path;
}

void OriAssistant::checkForUpdates(bool silent) {
    #ifdef CURL_FOUND
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string version_url = "https://raw.githubusercontent.com/piratheon/ORI/refs/heads/main/.version";
        std::string remote_version;
        curl_easy_setopt(curl, CURLOPT_URL, version_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &remote_version);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            std::ifstream version_file(".version");
            std::string current_version = ORI_VERSION;
            if (version_file.is_open()) {
                std::getline(version_file, current_version);
                version_file.close();
            }
            if (current_version.empty()) current_version = ORI_VERSION;
            remote_version.erase(remote_version.find_last_not_of(" \n\r\t")+1);
            if (current_version != remote_version) {
                if (silent) {
                    std::cout << YELLOW << "A new version of Ori is available: " << remote_version << RESET << std::endl;
                    std::cout << "Run " << BOLD << "ori --check-for-updates" << RESET << " to update." << std::endl;
                } else {
                    std::cout << YELLOW << "A new version of Ori is available: " << remote_version << RESET << std::endl;
                    std::cout << "Do you want to update? (y/n): ";
                    std::string confirmation;
                    std::getline(std::cin, confirmation);
                    if (confirmation == "y" || confirmation == "Y") {
                        std::string download_url = "https://github.com/piratheon/ORI/releases/download/v" + remote_version + "/ori-linux_x86-64-v" + remote_version + ".bin";
                        std::string temp_file = "/tmp/ori_update.bin";
                        CURL* download_curl = curl_easy_init();
                        if (download_curl) {
                            FILE* fp = fopen(temp_file.c_str(), "wb");
                            if (fp) {
                                curl_easy_setopt(download_curl, CURLOPT_URL, download_url.c_str());
                                curl_easy_setopt(download_curl, CURLOPT_WRITEFUNCTION, NULL);
                                curl_easy_setopt(download_curl, CURLOPT_WRITEDATA, fp);
                                CURLcode download_res = curl_easy_perform(download_curl);
                                fclose(fp);
                                if (download_res == CURLE_OK) {
                                    chmod(temp_file.c_str(), 0755);
                                    if (rename(temp_file.c_str(), executable_path.c_str()) == 0) {
                                        std::cout << GREEN << "Update successful! Restarting Ori..." << RESET << std::endl;
                                        char* const argv[] = {const_cast<char*>(executable_path.c_str()), NULL};
                                        execv(executable_path.c_str(), argv);
                                    } else {
                                        std::cout << RED << "Failed to replace the old binary." << RESET << std::endl;
                                    }
                                } else {
                                    std::cout << RED << "Failed to download the update." << RESET << std::endl;
                                }
                            }
                            curl_easy_cleanup(download_curl);
                        }
                    }
                }
            }
        }
    }
    #endif
}

void OriAssistant::showHelp() {
    std::cout << "Available commands:\n";
    std::cout << "  /help          - Show this help message\n";
    std::cout << "  /quit          - Exit the assistant\n";
    std::cout << "  /exit          - Exit the assistant\n";
    std::cout << "  /clear         - Clear the screen\n";
    std::cout << "  /cat [file]    - Print file content and add it to the chat context\n";
    std::cout << "  /exec [cmd]    - Execute a shell command and add the output to the chat context\n";
    std::cout << "  /autoexec [m]  - Set auto-execution mode for commands (ask, yes, no)\n";
    std::cout << "  /cmdoutput [o] - Toggle command output display (on, off)\n";
    std::cout << "  /init          - Initialize this directory as an Ori project (enables git snapshots for /undo)\n";
    std::cout << "  /gitbackup [o] - Toggle pre-edit git snapshots (on, off); only active in projects set up with /init\n";
    std::cout << "  /undo          - Restore the files Ori changed in the last response and remove that prompt from history\n";
    std::cout << "  /model [id]    - Switch to a different API model configuration by ID\n";
    std::cout << "  /skills        - Manage agent skills (list, show, learn, forget)\n";
    std::cout << "  /rag           - Manage RAG memory (on, off, add, search, clear)\n";
    std::cout << "  /agents        - Show loaded AGENTS.md rules and reload from workspace\n";
    std::cout << "  /task          - Manage task dispatcher (list, decompose, run, create, clear)\n";
    std::cout << "  /subagent      - List registered subagents\n";
    std::cout << "  Or type any query to send to the AI assistant\n\n";
    std::cout << "KEYBINDINGS & UX:\n";
    std::cout << "  TAB            - Auto-complete commands and file paths\n";
    std::cout << "  Ctrl+F         - Toggle command execution log\n";
    std::cout << "  Ctrl+C / ESC   - Cancel running command or clear prompt\n";
}
