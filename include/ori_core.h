#ifndef ORI_CORE_H
#define ORI_CORE_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <map>
#include "ori_provider.h"
#include "ori_agents.h"
#include "ori_task.h"
#include "ori_skills.h"
#include <json/json.h> // Include for Json::Value

#ifdef CURL_FOUND
#include <curl/curl.h>
#endif

struct ChatMessage {
    std::string role;
    std::string content;
};

struct Config {
    int port;
    bool no_banner;
    bool no_clear;
    std::string active_api_config;
    std::string auto_execute_commands_mode; // Added for /autoexec command
    bool debug; // Added debug flag
    bool rag_memory_enabled;
    bool skills_memory_enabled;
    bool show_command_output;

    Config();
};

extern bool g_debug_enabled_in_gui_mode; // Global flag for GUI debug logging
extern bool g_is_gui_mode; // Global flag for GUI mode
extern bool g_is_interactive_mode; // Global flag for interactive mode

extern std::atomic<bool> keep_running;
extern void run_spinner(const std::string& message);
extern void sigint_handler(int signum);
extern std::string colorize(const std::string& color, const std::string& text);

// ANSI Color Codes (declared extern for main.cpp usage)
extern const std::string RESET;
extern const std::string BOLD;
extern const std::string RED;
extern const std::string GREEN;
extern const std::string YELLOW;
extern const std::string BLUE;
extern const std::string MAGENTA;
extern const std::string CYAN;

bool has_pkexec();
std::string prepare_elevated_command(const std::string& cmd);

class ConfigManager {
private:
    std::string config_path;

public:
    ConfigManager();
    void loadConfig(Config& config);
    void saveConfig(const Config& config);
    void loadExternalConfig(Config& config, const std::string& path);
    void updateConfig(const std::string& key, const std::string& value);
    // Return a single config value as string (empty if not found)
    std::string getConfigValue(const std::string& key);
    // Return a JSON string containing all config values
    std::string getAllConfig();
};

struct CommandLogEntry {
    std::string command;
    std::string output;
};

struct ProviderInfo {
    std::unique_ptr<APIProvider> provider;
    Json::Value details; // Stores the full JSON entry from keys.json
};

struct UndoSnapshot {
    size_t history_size = 0;
    std::string git_commit_hash;
    bool has_git_commit = false;
};

class OriAssistant {
private:
    std::string executable_path;
    const size_t BANNER_HEIGHT = 12;  // Height of the banner in lines
    size_t current_output_lines = 0;  // Track number of lines output
    std::vector<CommandLogEntry> command_log;
    bool show_command_log = false;
    void displayCommandLog();
    void showBanner();
    std::string pre_prompt_context;
    
    std::map<std::string, ProviderInfo> providers_info;
    APIProvider* active_provider = nullptr;
    std::vector<ChatMessage> conversation_history;
    std::vector<UndoSnapshot> undo_snapshots;


public:
    Config config;
    ConfigManager configManager;
    AgentsManager agentsManager;
    TaskDispatcher taskDispatcher;
    SkillsManager skillsManager;
    RAGMemory ragMemory;
    
public:
        
        static std::atomic<bool> interrupted_flag;
    OriAssistant();
    ~OriAssistant();
    
    void setExecutablePath(const std::string& path);
    bool initialize();
    void run();
    void showHelp();
    std::string readInput();
    void processSingleRequest(const std::string& prompt, bool auto_confirm);
    void handleCommandExecution(const std::string& command, bool auto_confirm, bool send_to_ai = true);
    void handleResponse(const std::string& response, bool auto_confirm);
    void checkForUpdates(bool silent);
    void setSystemPrompt(const std::string& prompt);
    std::string sendQuery(const std::string& prompt);
    bool gitBackupCommit(std::string& out_commit_hash);
    bool performUndo();
};

#endif // ORI_CORE_H