#include "orpm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <cstring>

#ifdef JSONCPP_FOUND
#include <json/json.h>
#endif

Orpm::Orpm() {
    // Set default plugin directories
    const char* home_dir = std::getenv("HOME");
    if (home_dir != nullptr) {
        user_plugin_dir = std::string(home_dir) + "/.local/share/Ori/plugins";
    }
    system_plugin_dir = "/usr/share/Ori/plugins";
}

Orpm::~Orpm() {
    // Destructor
}

bool Orpm::initialize() {
    // Create user plugin directory if it doesn't exist
    struct stat st;
    if (stat(user_plugin_dir.c_str(), &st) == -1) {
        mkdir(user_plugin_dir.c_str(), 0755);
    }
    
    // Load plugin database
    loadPluginDatabase();
    return true;
}

void Orpm::loadPluginDatabase() {
#ifdef JSONCPP_FOUND
    // Load from JSON file
    std::ifstream file("plugins/plugin_database.json");
    if (!file.is_open()) {
        // Try to load from home directory
        const char* home_dir = std::getenv("HOME");
        if (home_dir != nullptr) {
            std::string home_file = std::string(home_dir) + "/.config/ori/plugin_database.json";
            file.open(home_file);
        }
    }
    
    if (file.is_open()) {
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(file, root)) {
            const Json::Value plugins = root["plugins"];
            for (const auto& plugin : plugins) {
                PluginInfo info;
                info.name = plugin["name"].asString();
                info.version = plugin["version"].asString();
                info.description = plugin["description"].asString();
                info.developer = plugin["developer"].asString();
                info.git_url = plugin["git_url"].asString();
                info.download_url = plugin["download_url"].asString();
                info.installed_version = "";
                info.is_installed = false;
                
                available_plugins[info.name] = info;
            }
        }
        file.close();
    }
#else
    // If JSON library is not available, populate with some example plugins
    PluginInfo voice_chat;
    voice_chat.name = "voice-chat";
    voice_chat.version = "1.1.2";
    voice_chat.description = "Voice interaction plugin for Ori Assistant";
    voice_chat.developer = "Ori Team";
    voice_chat.git_url = "https://github.com/ori-assistant/voice-chat-plugin";
    voice_chat.download_url = "https://github.com/ori-assistant/voice-chat-plugin/releases/download/v1.1.2/voice-chat.tar.xz";
    voice_chat.installed_version = "";
    voice_chat.is_installed = false;
    
    available_plugins[voice_chat.name] = voice_chat;
    
    PluginInfo shell_access;
    shell_access.name = "shell-access";
    shell_access.version = "1.2.0";
    shell_access.description = "Shell command execution with safety features";
    shell_access.developer = "Ori Team";
    shell_access.git_url = "https://github.com/ori-assistant/shell-access-plugin";
    shell_access.download_url = "https://github.com/ori-assistant/shell-access-plugin/releases/download/v1.2.0/shell-access.tar.xz";
    shell_access.installed_version = "";
    shell_access.is_installed = false;
    
    available_plugins[shell_access.name] = shell_access;
#endif
    
    // Check which plugins are actually installed
    auto installed_plugins = listInstalledPlugins();
    for (const auto& plugin_info : installed_plugins) {
        if (available_plugins.find(plugin_info.name) != available_plugins.end()) {
            available_plugins[plugin_info.name].is_installed = true;
            available_plugins[plugin_info.name].installed_version = available_plugins[plugin_info.name].version;
        }
    }
}

void Orpm::savePluginDatabase() {
    // In a real implementation, this would save the plugin database to a file
    // For now, this is a placeholder
}

std::vector<PluginInfo> Orpm::listAvailablePlugins() {
    std::vector<PluginInfo> plugins;
    for (const auto& pair : available_plugins) {
        plugins.push_back(pair.second);
    }
    return plugins;
}

std::vector<PluginInfo> Orpm::listInstalledPlugins() {
    std::vector<PluginInfo> plugins;
    
    // Check user plugin directory
    DIR* dir = opendir(user_plugin_dir.c_str());
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR && 
                strcmp(entry->d_name, ".") != 0 && 
                strcmp(entry->d_name, "..") != 0) {
                
                PluginInfo info;
                info.name = entry->d_name;
                info.is_installed = true;
                plugins.push_back(info);
            }
        }
        closedir(dir);
    }
    
    // Check system plugin directory
    dir = opendir(system_plugin_dir.c_str());
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR && 
                strcmp(entry->d_name, ".") != 0 && 
                strcmp(entry->d_name, "..") != 0) {
                
                // Check if already in list (user plugins take precedence)
                bool found = false;
                for (const auto& plugin : plugins) {
                    if (plugin.name == entry->d_name) {
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    PluginInfo info;
                    info.name = entry->d_name;
                    info.is_installed = true;
                    plugins.push_back(info);
                }
            }
        }
        closedir(dir);
    }
    
    return plugins;
}

bool Orpm::installPlugin(const std::string& plugin_name) {
    auto it = available_plugins.find(plugin_name);
    if (it == available_plugins.end()) {
        std::cerr << "Plugin '" << plugin_name << "' not found in repository.\n";
        return false;
    }
    
    PluginInfo& plugin = it->second;
    
    // In a real implementation, this would:
    // 1. Download the plugin from plugin.download_url
    // 2. Extract the tar.xz file
    // 3. Move files to the appropriate plugin directory
    // 4. Set proper permissions
    // 5. Update plugin database
    
    std::cout << "Installing plugin '" << plugin_name << "' version " << plugin.version << "...\n";
    std::cout << "Download URL: " << plugin.download_url << "\n";
    std::cout << "This is a placeholder implementation. In a real system, this would:\n";
    std::cout << "  1. Download the plugin from the URL\n";
    std::cout << "  2. Extract and install it to " << user_plugin_dir << "\n";
    std::cout << "  3. Update the plugin database\n";
    
    // Mark as installed for demonstration
    plugin.is_installed = true;
    plugin.installed_version = plugin.version;
    
    return true;
}

bool Orpm::removePlugin(const std::string& plugin_name) {
    auto it = available_plugins.find(plugin_name);
    if (it == available_plugins.end()) {
        std::cerr << "Plugin '" << plugin_name << "' not found.\n";
        return false;
    }
    
    PluginInfo& plugin = it->second;
    
    // In a real implementation, this would:
    // 1. Remove the plugin directory from user_plugin_dir
    // 2. Update plugin database
    
    std::cout << "Removing plugin '" << plugin_name << "'...\n";
    std::cout << "This is a placeholder implementation. In a real system, this would:\n";
    std::cout << "  1. Remove the plugin directory from " << user_plugin_dir << "\n";
    std::cout << "  2. Update the plugin database\n";
    
    // Mark as not installed for demonstration
    plugin.is_installed = false;
    plugin.installed_version = "";
    
    return true;
}

bool Orpm::updatePlugin(const std::string& plugin_name) {
    auto it = available_plugins.find(plugin_name);
    if (it == available_plugins.end()) {
        std::cerr << "Plugin '" << plugin_name << "' not found.\n";
        return false;
    }
    
    PluginInfo& plugin = it->second;
    
    if (!plugin.is_installed) {
        std::cerr << "Plugin '" << plugin_name << "' is not installed.\n";
        return false;
    }
    
    // In a real implementation, this would:
    // 1. Check if a newer version is available
    // 2. Download and install the update if available
    
    std::cout << "Updating plugin '" << plugin_name << "'...\n";
    std::cout << "This is a placeholder implementation. In a real system, this would:\n";
    std::cout << "  1. Check for newer versions\n";
    std::cout << "  2. Download and install updates if available\n";
    
    return true;
}

bool Orpm::searchPlugins(const std::string& query) {
    std::cout << "Searching for plugins matching '" << query << "':\n";
    
    bool found = false;
    for (const auto& pair : available_plugins) {
        const PluginInfo& plugin = pair.second;
        if (plugin.name.find(query) != std::string::npos || 
            plugin.description.find(query) != std::string::npos) {
            std::cout << "  " << plugin.name << " v" << plugin.version << " - " << plugin.description << "\n";
            found = true;
        }
    }
    
    if (!found) {
        std::cout << "  No plugins found matching your query.\n";
    }
    
    return found;
}

PluginInfo Orpm::getPluginInfo(const std::string& plugin_name) {
    auto it = available_plugins.find(plugin_name);
    if (it != available_plugins.end()) {
        return it->second;
    }
    
    PluginInfo empty;
    empty.name = plugin_name;
    empty.is_installed = false;
    return empty;
}

bool Orpm::isPluginInstalled(const std::string& plugin_name) {
    auto it = available_plugins.find(plugin_name);
    if (it != available_plugins.end()) {
        return it->second.is_installed;
    }
    return false;
}

std::string Orpm::getPluginPath(const std::string& plugin_name) {
    // Check if plugin is installed in user directory
    std::string user_path = user_plugin_dir + "/" + plugin_name;
    struct stat st;
    if (stat(user_path.c_str(), &st) == 0) {
        return user_path;
    }
    
    // Check if plugin is installed in system directory
    std::string system_path = system_plugin_dir + "/" + plugin_name;
    if (stat(system_path.c_str(), &st) == 0) {
        return system_path;
    }
    
    return "";
}
