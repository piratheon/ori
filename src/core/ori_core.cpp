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

#ifndef ORI_VERSION
#define ORI_VERSION "0.0"
#endif

std::atomic<bool> keep_running{true};
bool g_is_gui_mode = false;
bool g_debug_enabled_in_gui_mode = false; // New global flag for GUI debug logging
std::atomic<bool> OriAssistant::interrupted_flag{false};

void sigint_handler(int signum) {
    if (g_is_gui_mode) {
        exit(0); // Terminate the process if in GUI mode
    }
    OriAssistant::interrupted_flag = true;
}

void run_spinner(const std::string& message) {
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
    const char* display = std::getenv("DISPLAY");
    return display != nullptr && display[0] != '\0';
}

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

std::string OriAssistant::sendQuery(const std::string& prompt) {
    if (!active_provider) {
        return colorize(RED, "Error: No active API provider is configured.");
    }

    if (config.debug) {
        std::cerr << "[DEBUG] Active API Config / Model: " << config.active_api_config << "\n";
        std::cerr << "[DEBUG] Prompt: " << prompt << "\n";
    }
    
    conversation_history.push_back({"user", prompt});
    
    // Create a temporary copy of the conversation history for the provider
    std::vector<std::pair<std::string, std::string>> provider_history;
    for(const auto& msg : conversation_history) {
        provider_history.push_back({msg.role, msg.content});
    }

    std::string response = active_provider->sendQuery(prompt, provider_history);
    
    conversation_history.push_back({"assistant", response});
    
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
    // Destructor
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
            }
            else {
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
        // Find next tag: either [exec] or [edit]
        size_t exec_start = response.find("[exec]", current_pos);
        size_t exec_end = (exec_start != std::string::npos) ? response.find("[/exec]", exec_start) : std::string::npos;
        size_t edit_start = response.find("[edit]", current_pos);
        size_t edit_end = (edit_start != std::string::npos) ? response.find("[/edit]", edit_start) : std::string::npos;
        size_t writefile_start = response.find("[writefile(", current_pos);
        size_t writefile_end = (writefile_start != std::string::npos) ? response.find("[/writefile]", writefile_start) : std::string::npos;

        // Determine which tag comes next
        size_t next_pos = std::string::npos;
        enum TagType { NONE, EXEC, EDIT, WRITEFILE } next_tag = NONE;
        if (exec_start != std::string::npos && (edit_start == std::string::npos || exec_start < edit_start) && (writefile_start == std::string::npos || exec_start < writefile_start)) {
            next_pos = exec_start; next_tag = EXEC;
        } else if (edit_start != std::string::npos && (writefile_start == std::string::npos || edit_start < writefile_start)) {
            next_pos = edit_start; next_tag = EDIT;
        } else if (writefile_start != std::string::npos) {
            next_pos = writefile_start; next_tag = WRITEFILE;
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
                        OriEdit::applyChanges(op);
                    }
                }
            } else if (operation == "rename") {
                std::string filename = root.get("file", "").asString();
                std::string newname = root.get("newname", "").asString();
                if (filename.empty() || newname.empty()) {
                    std::cout << YELLOW << "[edit] rename requires 'file' and 'newname' fields" << RESET << std::endl;
                } else {
                    if (std::rename(filename.c_str(), newname.c_str()) == 0) {
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

            std::ofstream file(filename);
            if (file.is_open()) {
                file << content;
                file.close();
                std::cout << GREEN << "File created: " << filename << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to create file: " << filename << RESET << std::endl;
            }
            current_pos = writefile_end + strlen("[/writefile]");
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

void OriAssistant::handleCommandExecution(const std::string& command, bool auto_confirm, bool send_to_ai) {
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
            // Warn if sudo/su present but still ask for interactive confirmation
            if (command.find("sudo") != std::string::npos || command.find(" su ") != std::string::npos) {
                std::cout << YELLOW << "Warning: this command requests elevated privileges (contains 'sudo' or 'su'). It may prompt for a password when run." << RESET << std::endl;
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
            std::thread spinner_thread(run_spinner, "executing command...");

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
            spinner_thread.join();
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

        if (send_to_ai) {
            std::string feedback_prompt = "The command \"" + command + "\" produced the following output:\n---\n" + result + "\n---\nPlease summarize this output or answer the original question based on it.";
            processSingleRequest(feedback_prompt, auto_confirm);
        } else {
            std::cout << result << std::endl;
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
    std::cout << "  /autoexec [mode] - Set auto-execution mode for commands (ask, yes, no)\n";
    std::cout << "  /model [id]    - Switch to a different API model configuration by ID\n";
    std::cout << "  Or type any query to send to the AI assistant\n\n";
    std::cout << "KEYBINDINGS:\n";
    std::cout << "  Ctrl+F         - Toggle command execution log\n";
    std::cout << "  Ctrl+C / ESC   - Cancel running command or clear prompt\n";
}
