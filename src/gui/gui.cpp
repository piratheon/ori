#include "ori_gui.h"
#include "httplib.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "json/json.h"
#include "ori_core.h"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <filesystem>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define SERVER_CERT_FILE "cert.pem"
#define SERVER_PRIVATE_KEY_FILE "key.pem"

const std::string GUI_SYSTEM_PROMPT = R"ORI_PROMPT(About Me: Ori (GUI Mode)

I am Ori, an AI assistant created by the developper named Piratheon. I am designed to be a powerful and versatile assistant, operating within a web-based graphical user interface (GUI).

Core Capabilities

I can help you with development tasks through both direct commands and structured file operations. I can also display information in the GUI.

GUI Features:
- Code Canvas: I can display code snippets, diffs, and other text-based content in a dedicated "Canvas" area. To do this, use the `[canvas]` tag with the content to be displayed. For example: `[canvas]...my code...[/canvas]`.
- Command Execution: I can execute shell commands. To do this, use the `[exec]` tag with the command to be executed. For example: `[exec]ls -l[/exec]`.

File Operations
- Text Editing: Structured file modifications and content management
- Content Analysis: File content examination and search
- File Management: Organization and maintenance of files
- Comparison Tools: File comparison and difference analysis

Programming and Development
- Code Generation: I can write and edit code in numerous languages, including C/C++, Python, JavaScript, and more.
- Troubleshooting: I can help you debug code, analyze errors, and understand complex technical problems.
- Frameworks: I am familiar with a wide range of development frameworks and libraries.
)ORI_PROMPT";

extern const char _binary_index_html_start[];
extern const char _binary_index_html_end[];
extern const char _binary_favicon_svg_start[];
extern const char _binary_favicon_svg_end[];

// In-memory chat history
std::map<std::string, std::vector<std::pair<std::string, std::string>>> chat_sessions;
long long next_session_id = 0;

struct CommandInfo {
    pid_t pid;
    std::string log_path;
    int status;
};
std::map<std::string, CommandInfo> running_commands;
long long next_command_id = 0;

std::string generate_session_name(const std::string& prompt) {
    std::stringstream ss(prompt);
    std::string word1, word2;
    ss >> word1 >> word2;
    if (word2.empty()) {
        return word1;
    }
    return word1 + " " + word2;
}

void ori::start_gui()
{
    httplib::Server svr;

    svr.set_mount_point("/", "./www");

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        // Prefer serving a local copy in ./www/index.html so offline libraries
        // placed in the `www/` directory are used. Fall back to the embedded
        // binary index if the file is not present.
        const std::string local_index = std::string("./www/index.html");
        std::ifstream f(local_index, std::ios::in | std::ios::binary);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            res.set_content(ss.str(), "text/html");
            return;
        }

        const size_t content_len = _binary_index_html_end - _binary_index_html_start;
        res.set_content(_binary_index_html_start, content_len, "text/html");
    });

    svr.Get("/favicon.svg", [](const httplib::Request&, httplib::Response& res) {
        const std::string local_icon = std::string("./www/favicon.svg");
        std::ifstream f(local_icon, std::ios::in | std::ios::binary);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            res.set_content(ss.str(), "image/svg+xml");
            return;
        }

        const size_t content_len = _binary_favicon_svg_end - _binary_favicon_svg_start;
        res.set_content(_binary_favicon_svg_start, content_len, "image/svg+xml");
    });
    
    svr.Get("/api/version", [](const httplib::Request &, httplib::Response &res) {
      Json::Value root;
      root["version"] = "1.0.0";
      res.set_content(root.toStyledString(), "application/json");
    });

    svr.Get("/api/models", [](const httplib::Request &, httplib::Response &res) {
        Json::Value models(Json::arrayValue);
        models.append("x-ai/grok-4.1-fast:free");
        models.append("cognitivecomputations/dolphin-mistral-24b-venice-edition:free");
        models.append("qwen/qwen3-coder:free");
        models.append("alibaba/tongyi-deepresearch-30b-a3b:free");
        models.append("tngtech/deepseek-r1t2-chimera:free");
        res.set_content(models.toStyledString(), "application/json");
    });
    
    svr.Get("/api/chats", [](const httplib::Request &, httplib::Response &res) {
        Json::Value root(Json::arrayValue);
        for (const auto& session : chat_sessions) {
            Json::Value item;
            item["id"] = session.first;
            item["name"] = generate_session_name(session.second.front().first);
            root.append(item);
        }
        res.set_content(root.toStyledString(), "application/json");
    });

    svr.Get("/api/history", [](const httplib::Request &req, httplib::Response &res) {
        std::string session_id = req.get_param_value("session_id");
        Json::Value root(Json::arrayValue);
        if (chat_sessions.count(session_id)) {
            for (const auto& entry : chat_sessions[session_id]) {
                Json::Value item;
                item["user"] = entry.first;
                item["bot"] = entry.second;
                root.append(item);
            }
        }
        res.set_content(root.toStyledString(), "application/json");
    });

    svr.Get("/api/clear_chats", [](const httplib::Request &, httplib::Response &res) {
        chat_sessions.clear();
        next_session_id = 0;
        res.set_content("{}", "application/json");
    });

    svr.Post("/api/prompt", [](const httplib::Request &req, httplib::Response &res) {
        Json::Value root;
        Json::Reader reader;
        reader.parse(req.body, root);
        std::string prompt = root["prompt"].asString();
        std::string session_id = root["session_id"].asString();
        std::string model = root["model"].asString();

        if (session_id.empty()) {
            session_id = std::to_string(next_session_id++);
        }

        OriAssistant assistant;
        assistant.api->setSystemPrompt(GUI_SYSTEM_PROMPT);
        if (!model.empty()) {
            assistant.api->setModel(model);
        }
        assistant.api->setIsGui(true);
        if (!assistant.initialize()) {
            Json::Value err;
            err["error"] = "Failed to initialize assistant";
            res.set_content(err.toStyledString(), "application/json");
            return;
        }

        std::string response = assistant.api->sendQuery(prompt);
        
        chat_sessions[session_id].push_back({prompt, response});

        Json::Value result;
        if (response.rfind("API Error", 0) == 0) {
            result["error"] = response;
        } else {
            result["response"] = response;
        }
        result["session_id"] = session_id;

        res.set_content(result.toStyledString(), "application/json");
    });

    svr.Post("/api/exec", [](const httplib::Request &req, httplib::Response &res) {
        Json::Value root;
        Json::Reader reader;
        reader.parse(req.body, root);
        std::string command = root["command"].asString();
        std::string command_id = std::to_string(next_command_id++);
        std::string log_path = "/tmp/ori_exec_" + command_id + ".log";
        
        pid_t pid = fork();
        if (pid == -1) {
            Json::Value err;
            err["error"] = "Failed to fork process";
            res.set_content(err.toStyledString(), "application/json");
            return;
        } else if (pid == 0) { // Child process
            int log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (log_fd == -1) {
                exit(1);
            }
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
            execl("/bin/sh", "sh", "-c", command.c_str(), NULL);
            exit(127); // If execl fails
        } else { // Parent process
            running_commands[command_id] = {pid, log_path, 0};
            Json::Value result;
            result["command_id"] = command_id;
            res.set_content(result.toStyledString(), "application/json");
        }
    });

    svr.Get("/api/exec_log", [](const httplib::Request &req, httplib::Response &res) {
        std::string command_id = req.get_param_value("command_id");
        if (running_commands.count(command_id)) {
            std::ifstream log_file(running_commands[command_id].log_path);
            std::string log_content((std::istreambuf_iterator<char>(log_file)), std::istreambuf_iterator<char>());
            int status;
            pid_t result = waitpid(running_commands[command_id].pid, &status, WNOHANG);
            Json::Value root;
            root["log"] = log_content;
            if (result == running_commands[command_id].pid) {
                root["status"] = "finished";
                running_commands.erase(command_id);
            } else {
                root["status"] = "running";
            }
            res.set_content(root.toStyledString(), "application/json");
        } else {
            Json::Value err;
            err["error"] = "Invalid command ID";
            res.set_content(err.toStyledString(), "application/json");
        }
    });

    svr.Post("/api/exec_kill", [](const httplib::Request &req, httplib::Response &res) {
        Json::Value root;
        Json::Reader reader;
        reader.parse(req.body, root);
        std::string command_id = root["command_id"].asString();

        if (running_commands.count(command_id)) {
            kill(running_commands[command_id].pid, SIGKILL);
            running_commands.erase(command_id);
            Json::Value result;
            result["status"] = "killed";
            res.set_content(result.toStyledString(), "application/json");
        } else {
            Json::Value err;
            err["error"] = "Invalid command ID";
            res.set_content(err.toStyledString(), "application/json");
        }
    });

    auto serve_local = [](const std::string &req_path, httplib::Response &res) -> bool {
        std::string path = req_path;
        if (path == "/") path = "/index.html";
        std::string file_path = std::string("./www") + path;
        try {
            if (std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path)) {
                std::ifstream f(file_path, std::ios::in | std::ios::binary);
                if (!f) { res.status = 500; res.set_content("Failed to open file", "text/plain"); return true; }
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                std::string mime = "application/octet-stream";
                auto ext_pos = file_path.find_last_of('.');
                if (ext_pos != std::string::npos) {
                    std::string ext = file_path.substr(ext_pos + 1);
                    if (ext == "html") mime = "text/html";
                    else if (ext == "js") mime = "application/javascript";
                    else if (ext == "css") mime = "text/css";
                    else if (ext == "svg") mime = "image/svg+xml";
                    else if (ext == "png") mime = "image/png";
                    else if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";
                    else if (ext == "json") mime = "application/json";
                    else if (ext == "wasm") mime = "application/wasm";
                }
                res.set_content(content, mime);
                return true;
            }
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(std::string("Filesystem error: ") + e.what(), "text/plain");
            return true;
        }
        return false;
    };

    // Serve common asset prefixes using the same local-first logic as index/favicon
    svr.Get(R"(/js/(.*))", [&](const httplib::Request &req, httplib::Response &res){ if (serve_local(req.path, res)) return; res.status = 404; res.set_content("Not Found", "text/plain"); });
    svr.Get(R"(/css/(.*))", [&](const httplib::Request &req, httplib::Response &res){ if (serve_local(req.path, res)) return; res.status = 404; res.set_content("Not Found", "text/plain"); });
    svr.Get(R"(/lib/(.*))", [&](const httplib::Request &req, httplib::Response &res){ if (serve_local(req.path, res)) return; res.status = 404; res.set_content("Not Found", "text/plain"); });
    svr.Get(R"(/vendor/(.*))", [&](const httplib::Request &req, httplib::Response &res){ if (serve_local(req.path, res)) return; res.status = 404; res.set_content("Not Found", "text/plain"); });
    svr.Get(R"(/assets/(.*))", [&](const httplib::Request &req, httplib::Response &res){ if (serve_local(req.path, res)) return; res.status = 404; res.set_content("Not Found", "text/plain"); });

    // Generic static file fallback: serve files from ./www for any other path
    svr.Get(R"(/(.*))", [](const httplib::Request &req, httplib::Response &res) {
        std::string req_path = req.path;
        if (req_path == "/") req_path = "/index.html";
        std::string file_path = std::string("./www") + req_path;

        try {
            if (std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path)) {
                std::ifstream f(file_path, std::ios::in | std::ios::binary);
                if (!f) { res.status = 500; res.set_content("Failed to open file", "text/plain"); return; }
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                std::string mime = "application/octet-stream";
                auto ext_pos = file_path.find_last_of('.');
                if (ext_pos != std::string::npos) {
                    std::string ext = file_path.substr(ext_pos + 1);
                    if (ext == "html") mime = "text/html";
                    else if (ext == "js") mime = "application/javascript";
                    else if (ext == "css") mime = "text/css";
                    else if (ext == "svg") mime = "image/svg+xml";
                    else if (ext == "png") mime = "image/png";
                    else if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";
                    else if (ext == "json") mime = "application/json";
                    else if (ext == "wasm") mime = "application/wasm";
                }
                res.set_content(content, mime);
                return;
            }
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(std::string("Filesystem error: ") + e.what(), "text/plain");
            return;
        }

        res.status = 404;
        res.set_content("Not Found", "text/plain");
    });

    std::cout << "Starting server on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);
}