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

static std::atomic<bool> keep_running{true};
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

OpenRouterAPI::OpenRouterAPI() {
    // Constructor
    model = "google/gemini-2.0-flash-exp:free";
}

OpenRouterAPI::~OpenRouterAPI() {
    // Destructor
}

void OpenRouterAPI::setModel(const std::string& model_name) {
    model = model_name;
}

std::string OpenRouterAPI::getMotherboardFingerprint() {
    // Build a fingerprint from multiple non-privileged sources. Order of preference:
    // 1. /etc/machine-id
    // 2. /sys/class/dmi/id/product_uuid
    // 3. first non-loopback MAC address from /sys/class/net/*/address
    // Concatenate available identifiers with ':' and return the result.

    auto read_trimmed = [](const std::string& path) -> std::string {
        std::ifstream f(path);
        if (!f) return "";
        std::string s;
        if (!std::getline(f, s)) return "";
        // trim
        size_t end = s.find_last_not_of(" \n\r\t");
        if (end == std::string::npos) return "";
        size_t start = s.find_first_not_of(" \n\r\t");
        if (start == std::string::npos) start = 0;
        return s.substr(start, end - start + 1);
    };

    std::vector<std::string> parts;

    // /etc/machine-id
    std::string machine_id = read_trimmed("/etc/machine-id");
    if (!machine_id.empty()) parts.push_back(machine_id);

    // DMI product UUID (may be readable without sudo on many systems)
    std::string product_uuid = read_trimmed("/sys/class/dmi/id/product_uuid");
    if (!product_uuid.empty()) parts.push_back(product_uuid);

    // Try first non-loopback MAC from /sys/class/net
    DIR* d = opendir("/sys/class/net");
    if (d) {
        struct dirent* entry;
        while ((entry = readdir(d)) != NULL) {
            std::string ifname = entry->d_name;
            if (ifname == "." || ifname == ".." || ifname == "lo") continue;
            std::string addr_path = std::string("/sys/class/net/") + ifname + "/address";
            std::string mac = read_trimmed(addr_path);
            if (mac.empty()) continue;
            // skip all-zero MACs
            if (mac.find_first_not_of("0:") == std::string::npos) continue;
            parts.push_back(mac);
            break;
        }
        closedir(d);
    }

    // Join parts
    std::string fingerprint;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) fingerprint += ":";
        fingerprint += parts[i];
    }

    return fingerprint;
}

void OpenRouterAPI::setSystemPrompt(const std::string& prompt) {
    // Directly set the system prompt provided by the caller. The prompt should
    // be a plain-text instruction (no external file loading).
    conversation_history.clear();
    conversation_history.push_back({"system", prompt});
}

bool OpenRouterAPI::loadApiKey() {
    // Try environment variable first
    const char* env_key = std::getenv("OPENROUTER_API_KEY");
    if (env_key != nullptr && std::strlen(env_key) > 0) {
        api_key = std::string(env_key);
        return true;
    }

    // Then try a plaintext key file (~/.config/ori/key)
    const char* home_dir = std::getenv("HOME");
    if (home_dir != nullptr) {
        std::string key_file = std::string(home_dir) + "/.config/ori/key";
        std::ifstream file(key_file);
        if (file.is_open()) {
            std::string stored_key;
            std::getline(file, stored_key);
            file.close();
            if (!stored_key.empty()) {
                api_key = stored_key;
                return true;
            }
        }
    }

    // Prompt the user for the API key and persist it plainly with secure permissions.
    std::cout << "Please enter your OpenRouter API key: ";
    std::string key;
    std::getline(std::cin, key);
    if (key.empty()) return false;

    api_key = key;
    if (home_dir != nullptr) {
        std::string key_file = std::string(home_dir) + "/.config/ori/key";
        std::ofstream file(key_file);
        if (file.is_open()) {
            file << key;
            file.close();
            chmod(key_file.c_str(), 0600);
        }
    }
    return true;
}

bool OpenRouterAPI::setApiKey(const std::string& key) {
    api_key = key;
    return true;
}

std::string OpenRouterAPI::getApiKey() const {
    return api_key;
}

void OpenRouterAPI::setIsGui(bool isGui) {
    m_isGui = isGui;
    g_is_gui_mode = isGui;
}

std::string OpenRouterAPI::colorize(const std::string& color, const std::string& text) {
    if (m_isGui) {
        return text;
    }
    return color + text + RESET;
}

std::string OpenRouterAPI::sendQuery(const std::string& prompt) {
#ifdef CURL_FOUND
    // Add user's message to history
    conversation_history.push_back({"user", prompt});

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        return colorize(RED, "Error: Failed to initialize curl");
    }
    
    // Prepare the request data
    Json::Value request_data;
    request_data["model"] = model;
    
    Json::Value messages(Json::arrayValue);
    for (const auto& msg : conversation_history) {
        Json::Value message;
        message["role"] = msg.role;
        message["content"] = msg.content;
        messages.append(message);
    }
    request_data["messages"] = messages;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // Compact output
    std::string json_data = Json::writeString(builder, request_data);

    // Debug: optionally print the outgoing JSON payload so we can verify the system prompt
    const char* debug_env = std::getenv("ORI_DEBUG");
    if (debug_env && std::string(debug_env) == "1") {
        std::cerr << "[ORI_DEBUG] Outgoing JSON payload:\n" << json_data << std::endl;
        std::cerr << "[ORI_DEBUG] conversation_history roles: ";
        for (const auto& msg : conversation_history) std::cerr << msg.role << ",";
        std::cerr << std::endl;
    }
    
    // Set up curl options
    std::string response_data;
    struct curl_slist* headers = NULL;
    
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://openrouter.ai/api/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OriAssistant/1.0");
    
    // Perform the request with retry logic
    const int max_retries = 5;
    const int retry_delay_seconds = 2;
    CURLcode res = CURLE_OK;
    long http_code = 0;

    for (int attempt = 0; attempt < max_retries; ++attempt) {
        std::string spinner_message = "loading...";
        if (attempt > 0) {
            std::string reason;
            if (res != CURLE_OK) {
                reason = curl_easy_strerror(res);
            } else if (http_code == 429) {
                reason = "rate limited";
            } else {
                reason = "connection failed";
            }
            spinner_message = reason + ", retrying...";
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
        }

        response_data.clear();
        keep_running = true;
        std::thread spinner_thread(run_spinner, spinner_message);
        res = curl_easy_perform(curl);
        keep_running = false;
        spinner_thread.join();

        if (res != CURLE_OK) {
            if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST || res == CURLE_OPERATION_TIMEDOUT) {
                if (attempt < max_retries - 1) continue; // Retry on specific connection errors
            }
            break; // Don't retry on other curl errors or on last attempt
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code >= 200 && http_code < 300) {
            break; // Success
        }

        if (http_code == 429) { // Rate limit
            if (attempt < max_retries - 1) continue; // Retry
        }

        // For any other http_code, break and fail.
        break;
    }
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return colorize(RED, "Error: Failed to connect to OpenRouter API - " + std::string(curl_easy_strerror(res)));
    }
    
    // Parse the response
    Json::Value response_json;
    Json::Reader reader;
    if (!reader.parse(response_data, response_json)) {
        return colorize(RED, "Error: Failed to parse API response - " + response_data);
    }
    
    // Check for a structured error response
    if (response_json.isMember("error")) {
        std::string error_message = "API Error";
        const Json::Value& error_obj = response_json["error"];

        if (error_obj.isMember("code") && error_obj["code"].isNumeric()) {
            error_message += " (Code: " + error_obj["code"].asString() + ")";
        }
        if (error_obj.isMember("message") && error_obj["message"].isString()) {
            error_message += ": " + error_obj["message"].asString();
        }
        if (error_obj.isMember("metadata") && error_obj["metadata"].isObject() &&
            error_obj["metadata"].isMember("raw") && error_obj["metadata"]["raw"].isString()) {
            error_message += " (Details: " + error_obj["metadata"]["raw"].asString() + ")";
        }
        return colorize(RED, error_message);
    }
    
    // Extract the response text
    if (response_json.isMember("choices") && response_json["choices"].isArray() && 
        response_json["choices"].size() > 0 && 
        response_json["choices"][0].isMember("message") && 
        response_json["choices"][0]["message"].isMember("content")) {
        
        std::string assistant_response = response_json["choices"][0]["message"]["content"].asString();
        // Add assistant's response to history
        conversation_history.push_back({"assistant", assistant_response});
        return assistant_response;
    } else {
        return colorize(RED, "Error: Unexpected API response format - " + response_data);
    }
#else
    // Fallback to a local stub that at least includes the system prompt and conversation
    // so that the system prompt is not ignored when libcurl is unavailable.
    std::ostringstream oss;
    oss << "[LocalStub] Conversation so far:\n\n";
    for (const auto& msg : conversation_history) {
        oss << "[" << msg.role << "]\n" << msg.content << "\n\n";
    }
    oss << "[user]\n" << prompt << "\n\n";
    oss << "[assistant]\n";
    // This is a stub response — in a full build with libcurl this would be the model output.
    oss << "(no model available in this build; install libcurl and enable CURL_FOUND to contact the API)";
    // Add assistant's response to history so subsequent calls see context
    conversation_history.push_back({"assistant", oss.str()});
    return oss.str();
#endif
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
    api = std::make_unique<OpenRouterAPI>();
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
    if (home_dir != nullptr) {
        std::string config_dir = std::string(home_dir) + "/.config/ori";
        struct stat st;
        if (stat(config_dir.c_str(), &st) == -1) {
            mkdir(config_dir.c_str(), 0755);
        }
    }
    
    configManager.loadConfig(config);
    api->setModel(config.model);

    if (!api->loadApiKey()) {
        std::cerr << RED << "Error: Failed to load API key. Please set OPENROUTER_API_KEY or create Openrouter_api_key.txt." << RESET << std::endl;
        return false;
    }
    return true;
}

void OriAssistant::run() {
    checkForUpdates(true);
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
                handleCommandExecution(command, true, false);
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
        std::cout << BOLD << BLUE << "ORI Terminal Assistant v1.1.3" << RESET << "\n";
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
                    OriEdit::EditOperation op;
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
        #include <filesystem>

// ... (rest of the file)

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
    if (!api) {
        std::cout << RED << "Error: API not initialized" << RESET << std::endl;
        return;
    }
    
    std::string full_prompt = prompt;
    if (!pre_prompt_context.empty()) {
        full_prompt = pre_prompt_context + "\n" + prompt;
        pre_prompt_context.clear();
    }
    
    // Get response and handle it
    handleResponse(api->sendQuery(full_prompt), auto_confirm);
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
                    api->sendQuery("User cancelled the command execution.");
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
        api->sendQuery("The user cancelled the command execution. Please inform the user that you cannot answer the question without running the command.");
    }
}

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
            std::string current_version = "1.1.3";
            if (version_file.is_open()) {
                std::getline(version_file, current_version);
                version_file.close();
            }
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
    std::cout << "  Or type any query to send to the AI assistant\n\n";
    std::cout << "KEYBINDINGS:\n";
    std::cout << "  Ctrl+F         - Toggle command execution log\n";
    std::cout << "  Ctrl+C / ESC   - Cancel running command or clear prompt\n";
}
