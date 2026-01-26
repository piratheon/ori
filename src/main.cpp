#include "ori_core.h"
#include "ori_gui.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>
#include <unordered_map>

const std::string SYSTEM_PROMPT = R"ORI_PROMPT(About Me: Ori

I am Ori, an AI assistant created by the developper named Piratheon. I am designed to be a powerful and versatile assistant, operating directly within your command-line interface to help you with a wide range of tasks.

Core Capabilities

My primary strength is my deep integration with the ORI system and the shell environment. I can help you with development tasks through both direct commands and structured file operations.

Command Execution
- Shell Integration: Execute system commands and scripts
- Process Management: Handle background processes and tasks
- System Operations: Perform system maintenance and checks

File Operations
- Text Editing: Structured file modifications and content management
- Content Analysis: File content examination and search
- File Management: Organization and maintenance of files
- Comparison Tools: File comparison and difference analysis

File System Operations
- File Management: I can read, write, search, and organize files and directories.
- Content Analysis: I can analyze file contents and extract information.
- Text Editing: I support structured text editing operations:
  - Pattern search and replace
  - Content modification and transformation
  - File comparison and analysis
  - Code refactoring and organization
  - File renaming and moving
  - Automated batch operations

Programming and Development
- Code Generation: I can write and edit code in numerous languages, including C/C++, Python, JavaScript, and more.
- Troubleshooting: I can help you debug code, analyze errors, and understand complex technical problems.
- Frameworks: I am familiar with a wide range of development frameworks and libraries.

General Problem Solving
- Research and Analysis: I can conduct research, summarize information, and analyze data to answer your questions.
- Structured Thinking: I break down complex problems into manageable steps and execute them methodically.

Command and Edit Operations

I support two types of operations:

1. Regular Command Execution
For regular shell commands and system operations, I use the [exec] tag.

Always return commands wrapped in the [exec]...[/exec] tag when you want Ori to run them.
Do NOT ask the user for explicit confirmation before returning an [exec] block — Ori will execute the command when it receives it.

If a command includes superuser escalation (for example it contains the word "sudo" or "su"), the assistant must still include the [exec] tag but should also include a short human-readable warning line explaining that the command requires elevated privileges and may prompt the user for a password. Ori itself will execute the command and will not ask for a separate "yes" confirmation; it will only warn the user about privilege implications.

Examples (assistant MUST return strictly escaped JSON for [edit] tags and must wrap shell commands in [exec]):
[exec]ls -la[/exec]
[exec]grep "pattern" file[/exec]
[exec]cat /etc/passwd[/exec]

2. ORI Text Edit Operations
For text editing and file modifications, I use the [edit] tag with JSON format. The [edit] block should contain the operation type, target file(s), and the content details or parameters.

Operations include: search, replace, modify, refactor, rename, compare.

Options that can be used with edits: preview, diff, backup, interactive, safe.

Examples of edit operations (JSON in plain text):
[edit]
{
    "operation": "replace",
    "file": "config.json",
    "content": { "old": "development", "new": "production" }
}
[/edit]

[edit]
{
    "operation": "compare",
    "files": ["file1.txt", "file2.txt"]
}
[/edit]

Each edit operation specifies the type of operation, target file(s), and content or parameters to modify or analyze.

3. ORI File Write Operations
For creating new files, I use the [writefile(filename)] tag. The content of the file should be placed between the [writefile(filename)] and [/writefile] tags.
You must use the `[writefile]` tag to create files.

Example of a writefile operation:
[writefile(example.txt)]
This is the content of the file.
[/writefile]

ORI Integration Features

Safety mechanisms, previews, backups, and diffs are implemented in the ORI codebase. Edit operations are handled by ORI and may create backups, show diffs, and manage changes according to the configured behavior.

How I Approach Tasks

When you give me a task, I follow a structured approach:
1. Analyze the request to understand requirements.
2. Plan the steps and choose appropriate tools.
3. Execute the plan and report results.

I am here to be your reliable partner in the terminal. Let me know what you need, and I will do my best to assist you.
)ORI_PROMPT";

void showUsage() {
    std::cout << "ORI Terminal Assistant v1.1.4 - Linux TUI AI Assistant\n";
    std::cout << "Usage: ori [options] [prompt]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  -g, --gui               Start the web UI\n";
    std::cout << "  -y, --yes               Auto-confirm any command execution prompts\n";
    std::cout << "  -c, --config <command>  Manage configuration\n";
    std::cout << "                            load <path>  Load a configuration from a file\n";
    std::cout << "                            set <key> <value>  Set a configuration value\n";
    std::cout << "                            cat <key|all>  Print a configuration value or all values\n";
    std::cout << "  --check-for-updates     Check for updates\n";
    std::cout << "  --no-banner             Load Ori without the ASCII banner\n";
    std::cout << "  --no-clear              Load Ori without clearing the terminal\n";
    std::cout << "  -m, --model <model_name>      Specify the AI model to use (overrides config)\n";
    std::cout << "  -p, --port <port_number>      Specify the port for the GUI (overrides config)\n";
    std::cout << "  -d, --debug             Enable debug logging\n"; // Added debug flag
    std::cout << "\nShell Integration Examples:\n";
    std::cout << "  ori -y 'install nmap for me'\n";
    std::cout << "  ori print current active username\n";
    std::cout << "\nIf no options are provided, the interactive assistant will start.\n";
}

void showVersion() {
    std::cout << "ORI Terminal Assistant v1.1.4\n";
}

void processDirectPrompt(OriAssistant& assistant, const std::string& prompt, bool auto_confirm) {
    // Get response directly without showing the prompt again
    std::string response = assistant.api->sendQuery(prompt);
    assistant.handleResponse(response, auto_confirm);
}



int main(int argc, char* argv[]) {
    std::string executable_path = argv[0];
    OriAssistant assistant;
    assistant.setExecutablePath(executable_path);
    assistant.api->setSystemPrompt(SYSTEM_PROMPT);
    if (!assistant.initialize()) {
        std::cerr << "Failed to initialize ORI Terminal Assistant. Please check your API key configuration.\n";
        return 1;
    }

    bool auto_confirm = false;
    bool gui_mode = false;
    bool port_specified = false;
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string prompt = "";
    int prompt_start_index = -1;

    std::unordered_map<std::string, std::function<int(int, const std::vector<std::string>&)>> arg_handlers;

    arg_handlers["-h"] = arg_handlers["--help"] = [&](int i, const std::vector<std::string>& args) {
        showUsage();
        return 0;
    };
    arg_handlers["-v"] = arg_handlers["--version"] = [&](int i, const std::vector<std::string>& args) {
        showVersion();
        return 0;
    };
    arg_handlers["-g"] = arg_handlers["--gui"] = [&](int i, const std::vector<std::string>& args) {
        gui_mode = true;
        return 1;
    };
    arg_handlers["-y"] = arg_handlers["--yes"] = [&](int i, const std::vector<std::string>& args) {
        auto_confirm = true;
        return 1;
    };
    arg_handlers["--no-banner"] = [&](int i, const std::vector<std::string>& args) {
        assistant.config.no_banner = true;
        return 1;
    };
    arg_handlers["--no-clear"] = [&](int i, const std::vector<std::string>& args) {
        assistant.config.no_clear = true;
        return 1;
    };
    arg_handlers["-d"] = arg_handlers["--debug"] = [&](int i, const std::vector<std::string>& args) {
        assistant.config.debug = true;
        return 1;
    };
    arg_handlers["--check-for-updates"] = [&](int i, const std::vector<std::string>& args) {
        assistant.checkForUpdates(false);
        return 0;
    };
    arg_handlers["-m"] = arg_handlers["--model"] = [&](int i, const std::vector<std::string>& args) {
        if (i + 1 < args.size()) {
            assistant.config.model = args[i + 1];
            return 2;
        }
        return 1;
    };
    arg_handlers["-p"] = arg_handlers["--port"] = [&](int i, const std::vector<std::string>& args) {
        if (i + 1 < args.size()) {
            assistant.config.port = std::stoi(args[i + 1]);
            port_specified = true;
            return 2;
        }
        return 1;
    };
    arg_handlers["-c"] = arg_handlers["--config"] = [&](int i, const std::vector<std::string>& args) {
        if (i + 1 < args.size()) {
            std::string config_cmd = args[i + 1];
            if (config_cmd == "load" && i + 2 < args.size()) {
                assistant.configManager.loadExternalConfig(assistant.config, args[i + 2]);
                return 3;
            } else if (config_cmd == "set" && i + 3 < args.size()) {
                assistant.configManager.updateConfig(args[i + 2], args[i + 3]);
                return 0;
            } else if (config_cmd == "cat") {
                if (i + 2 < args.size()) {
                    std::string key = args[i + 2];
                    if (key == "all") {
                        std::string all = assistant.configManager.getAllConfig();
                        std::cout << all << std::endl;
                    } else {
                        std::string val = assistant.configManager.getConfigValue(key);
                        if (val.empty()) {
                            std::cerr << "Unknown config key: " << key << std::endl;
                            return -1;
                        }
                        std::cout << val << std::endl;
                    }
                } else {
                    std::cout << "Available config keys: port, model, no_banner, no_clear, all" << std::endl;
                    std::cout << "Usage: --config cat <key|all>  (e.g. --config cat model)" << std::endl;
                }
                return 0;
            }
        }
        return 1;
    };

    for (int i = 0; i < args.size();) {
        std::string arg = args[i];
        auto it = arg_handlers.find(arg);
        if (it != arg_handlers.end()) {
            int result = it->second(i, args);
            if (result == 0) return 0;
            if (result == -1) return 1;
            i += result;
        } else {
            prompt_start_index = i;
            break;
        }
    }

    // Check for --port without --gui and issue a warning
    if (port_specified && !gui_mode) {
        std::cerr << YELLOW << "Warning: --port specified but --gui not enabled. Port setting will be ignored." << RESET << std::endl;
        std::cerr << YELLOW << "Please use -g or --gui to start the web UI." << RESET << std::endl;
        return 1; // Exit after warning
    }

    // Set global g_is_gui_mode *before* initialize() is called
    g_is_gui_mode = gui_mode;

    // Set global debug flag for GUI mode
    g_debug_enabled_in_gui_mode = assistant.config.debug && g_is_gui_mode;

    if (gui_mode) {
        ori::start_gui(assistant.config.port);
        return 0;
    }

    if (prompt_start_index != -1) {
        for (int i = prompt_start_index; i < args.size(); ++i) {
            if (!prompt.empty()) prompt += " ";
            prompt += args[i];
        }
    }

    if (!prompt.empty()) {
        processDirectPrompt(assistant, prompt, auto_confirm);
        return 0;
    }

    assistant.run();
    return 0;
}