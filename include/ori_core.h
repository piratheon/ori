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
    bool git_backup_enabled;

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

// A file that Ori created, modified or renamed while handling one response.
// /undo restores exactly these paths and nothing else.
struct UndoPath {
    std::string abs_path;        // canonical absolute path
    std::string rel_path;        // path relative to the repository root (valid when in_repo)
    bool existed_before = false; // false => Ori created it, so /undo removes it
    bool in_repo = true;         // false => outside the repository, cannot be restored via git
};

struct UndoSnapshot {
    size_t history_size = 0;
    std::string git_commit_hash;
    bool has_git_commit = false;
    std::string backup_ref;      // private ref (refs/ori/backups/...) keeping the snapshot alive
    std::string repo_root;       // repository the snapshot was taken in
    std::vector<UndoPath> paths; // paths touched by Ori since the snapshot
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

    // Git backup bookkeeping
    std::string backup_session_id;
    unsigned backup_counter = 0;
    bool backup_refs_pruned = false;
    bool git_hint_shown = false;
    void beginUndoSnapshot();
    UndoPath makeUndoPath(const std::string& path) const;
    void addUndoPath(const UndoPath& p);
    void deleteBackupRef(const std::string& repo_root, const std::string& ref);
    void pruneStaleBackupRefs(const std::string& repo_root);


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
    // Snapshots the working tree onto a private ref, without touching the user's
    // branch, index or git config. Returns false (and leaves out_commit_hash empty)
    // unless git backups are enabled AND the current directory is inside a git work
    // tree that was explicitly initialized with /init (or `ori --init`).
    bool gitBackupCommit(std::string& out_commit_hash, std::string* out_ref = nullptr, std::string* out_root = nullptr);
    // Initializes the current directory as an Ori project (creating a git repo only
    // after confirmation). Returns true on success or if already initialized.
    bool initProject(bool auto_confirm = false);
    bool isProjectInitialized() const;
    bool performUndo();
};

#endif // ORI_CORE_H