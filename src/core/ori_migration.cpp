#include "ori_migration.h"
#include "ori_core.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <filesystem>
#include <json/json.h>
#include <cstdlib>

namespace fs = std::filesystem;

// The original hash of the keys.json.example file.
// This is a simple checksum.
unsigned long calculate_checksum(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0;
    }
    unsigned long checksum = 0;
    char c;
    while (file.get(c)) {
        checksum += static_cast<unsigned char>(c);
    }
    return checksum;
}

const unsigned long ORIGINAL_EXAMPLE_CHECKSUM = 4353; // Checksum of the original keys.json.example

MigrationManager::MigrationManager(const std::string& config_dir, bool debug_enabled)
    : config_dir(config_dir),
      key_file_path(config_dir + "/key"),
      config_file_path(config_dir + "/config.json"),
      keys_json_path(config_dir + "/keys.json"),
      keys_json_example_path(config_dir + "/keys.json.example"),
      debug_enabled(debug_enabled) {}

void MigrationManager::run() {
    if (fs::exists(keys_json_path)) {
        return; // Already has keys.json, nothing to do.
    }

    if (fs::exists(keys_json_example_path) && isExampleModified()) {
        renameExample();
        return;
    }

    if (needsMigration()) {
        performMigration();
        return;
    }

    firstTimeSetup();
}

bool MigrationManager::needsMigration() {
    if (debug_enabled) std::cerr << "Debug: Checking if migration is needed." << std::endl;
    return fs::exists(key_file_path);
}

void MigrationManager::performMigration() {
    if (debug_enabled) std::cerr << "Debug: Performing migration." << std::endl;
    std::cout << "Old configuration found. Migrating to new keys.json format..." << std::endl;

    std::ifstream key_file(key_file_path);
    std::string api_key;
    if (key_file.is_open()) {
        std::getline(key_file, api_key);
    }

    std::string model = "google/gemini-2.0-flash-exp:free"; // default
    std::ifstream config_file(config_file_path);
    if (config_file.is_open()) {
        Json::Value root;
        config_file >> root;
        model = root.get("model", model).asString();
    }

    Json::Value key_entry;
    key_entry["provider"] = "openrouter";
    key_entry["api_key"] = api_key;
    key_entry["id"] = "migrated-openrouter";
    key_entry["role"] = "migrated";
    key_entry["model"] = model;

    Json::Value keys;
    keys.append(key_entry);

    std::ofstream keys_json_file(keys_json_path);
    keys_json_file << keys;

    fs::rename(key_file_path, key_file_path + ".migrated");

    std::cout << "Migration complete. Your new configuration is in " << keys_json_path << std::endl;
}

bool MigrationManager::isExampleModified() {
    if (!fs::exists(keys_json_example_path)) {
        return false;
    }
    unsigned long current_checksum = calculate_checksum(keys_json_example_path);
    if (debug_enabled) std::cerr << "Debug: Example checksum " << current_checksum << ", original " << ORIGINAL_EXAMPLE_CHECKSUM << std::endl;
    return current_checksum != ORIGINAL_EXAMPLE_CHECKSUM;
}

void MigrationManager::renameExample() {
    if (debug_enabled) std::cerr << "Debug: Renaming example file." << std::endl;
    std::cout << "Modified keys.json.example found. Renaming to keys.json..." << std::endl;
    fs::rename(keys_json_example_path, keys_json_path);
}

void MigrationManager::firstTimeSetup() {
    if (debug_enabled) std::cerr << "Debug: Running first time setup." << std::endl;
    std::cout << GREEN << "[+] Welcome to Ori! This looks like your first time." << RESET << std::endl;
    std::cout << CYAN << "[?] How would you like to configure Ori?" << RESET << std::endl;
    std::cout << "  1. Simple chatbot (guided setup)" << std::endl;
    std::cout << "  2. Advanced agentic system (manual configuration)" << std::endl;
    std::cout << CYAN << "[➜] Choice [1-2] (default: 1): " << RESET;
    std::string choice;
    std::getline(std::cin, choice);
    if (choice.empty()) {
        choice = "1";
    }

    if (choice == "1") {
        if (debug_enabled) std::cerr << "Debug: User chose Simple chatbot." << std::endl;
        std::cout << "\n" << CYAN << "[?] Select your API provider:" << RESET << std::endl;
        std::cout << "  1. Google" << std::endl;
        std::cout << "  2. Hugging Face" << std::endl;
        std::cout << "  3. OpenRouter" << std::endl;
        std::cout << CYAN << "[➜] Choice [1-3] (default: 1): " << RESET;
        std::string provider_choice;
        std::getline(std::cin, provider_choice);

        std::string provider;
        std::string default_model;
        if (provider_choice == "2") {
            provider = "huggingface";
            default_model = "gpt2";
        } else if (provider_choice == "3") {
            provider = "openrouter";
            default_model = "google/gemini-2.0-flash-exp:free";
        } else {
            provider = "google";
            default_model = "gemini-2.5-flash";
        }

        std::cout << CYAN << "[?] Enter preferred model (default: " << default_model << "): " << RESET;
        std::string selected_model;
        std::getline(std::cin, selected_model);
        if (selected_model.empty()) {
            selected_model = default_model;
        }

        std::string api_key;
        while (true) {
            std::cout << CYAN << "[?] Enter your API key for " << provider << ": " << RESET;
            std::getline(std::cin, api_key);
            if (!api_key.empty()) {
                break;
            }
            std::cout << RED << "[!] API key cannot be empty. Please try again." << RESET << std::endl;
        }

        std::string config_id = provider + "-default";

        Json::Value key_entry;
        key_entry["provider"] = provider;
        key_entry["api_key"] = api_key;
        key_entry["id"] = config_id;
        key_entry["role"] = "default";
        key_entry["model"] = selected_model;

        Json::Value keys;
        keys.append(key_entry);

        std::ofstream keys_json_file(keys_json_path);
        keys_json_file << keys;

        // Save active_api_config into config.json so Ori activates this provider automatically
        Config config;
        ConfigManager config_manager;
        config_manager.loadConfig(config);
        config.active_api_config = config_id;
        config_manager.saveConfig(config);

        std::cout << GREEN << "[✓] Configuration saved to " << keys_json_path << RESET << std::endl;

    } else if (choice == "2") {
        if (debug_enabled) std::cerr << "Debug: User chose Advanced agentic system." << std::endl;
        std::cout << "Opening keys.json.example for you to edit." << std::endl;
        
        // Ensure config_dir exists
        if (!fs::exists(config_dir)) {
            try {
                fs::create_directories(config_dir);
            } catch (const fs::filesystem_error& e) {
                if (debug_enabled) std::cerr << RED << "Error creating config directory: " << e.what() << RESET << std::endl;
                return;
            }
        }

        // Copy keys.json.example to config directory
        std::string project_root_example_path = "keys.json.example";
        if (!fs::exists(project_root_example_path)) {
            std::string sys_example_path = std::string(ORI_DATA_DIR) + "/keys.json.example";
            if (fs::exists(sys_example_path)) {
                project_root_example_path = sys_example_path;
            } else if (fs::exists("/usr/share/Ori/keys.json.example")) {
                project_root_example_path = "/usr/share/Ori/keys.json.example";
            }
        }
        if (!fs::exists(project_root_example_path)) {
            if (debug_enabled) std::cerr << RED << "Error: keys.json.example not found. Cannot create example config." << RESET << std::endl;
            return;
        }

        try {
            fs::copy(project_root_example_path, keys_json_example_path, fs::copy_options::overwrite_existing);
            std::cout << GREEN << "Example keys.json copied to: " << keys_json_example_path << RESET << std::endl;
        } catch (const fs::filesystem_error& e) {
            if (debug_enabled) std::cerr << RED << "Error copying keys.json.example: " << e.what() << RESET << std::endl;
            return;
        }

        // Verify copied file exists
        if (!fs::exists(keys_json_example_path)) {
            if (debug_enabled) std::cerr << RED << "Error: Copied keys.json.example not found at " << keys_json_example_path << RESET << std::endl;
            return;
        }

        std::string command_to_execute;
        const char* editor_env = getenv("EDITOR");

        // Prefer the EDITOR environment variable if set to a known terminal editor, otherwise try xdg-open, then nano
        if (editor_env && (std::string(editor_env).find("vim") != std::string::npos || std::string(editor_env).find("nano") != std::string::npos || std::string(editor_env).find("emacs") != std::string::npos)) {
            command_to_execute = std::string(editor_env) + " " + keys_json_example_path;
        } else if (system("which xdg-open > /dev/null 2>&1") == 0) {
            command_to_execute = "xdg-open " + keys_json_example_path;
        } else {
            command_to_execute = "nano " + keys_json_example_path;
            std::cout << YELLOW << "No preferred editor found (EDITOR environment variable not set to vim/nano/emacs, and xdg-open not available/configured). Falling back to nano. You can set the EDITOR environment variable for your preferred editor." << RESET << std::endl;
        }
        
        if (debug_enabled) std::cerr << YELLOW << "Executing command: " << command_to_execute << RESET << std::endl;
        system(command_to_execute.c_str());

        std::cout << "Please add your API keys and save the file as keys.json in " << config_dir << std::endl;
        std::cout << "Once you are done editing the file, save and close the editor, then press Enter here to continue..." << std::endl;
        std::string dummy;
        std::getline(std::cin, dummy); // Wait for user to finish editing and press Enter

        // After user presses Enter, check if keys.json was created
        if (!fs::exists(keys_json_path)) {
            if (debug_enabled) std::cerr << RED << "Error: keys.json not found after editing. Please create it manually in " << config_dir << RESET << std::endl;
            // Optionally, loop and re-prompt or exit
            return;
        }
    } // Closing brace for else if (choice == "2")
} // Closing brace for firstTimeSetup()
