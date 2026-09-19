#include "ori_core.h"
#include <fstream>
#include <json/json.h>
#include <sys/stat.h>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <iostream>

Config::Config() : port(8080), no_banner(false), no_clear(false), active_api_config("migrated-openrouter"), auto_execute_commands_mode("ask"), debug(false) {}

ConfigManager::ConfigManager() {
    const char* home_dir = getenv("HOME");
    if (home_dir != nullptr) {
        config_path = std::string(home_dir) + "/.config/ori/config.json";
    }
}

void ConfigManager::loadConfig(Config& config) {
    if (!std::filesystem::exists(config_path)) {
        saveConfig(config);
        return;
    }

    std::ifstream file(config_path);
    if (!file.is_open()) {
        return;
    }

    Json::Value root;
    file >> root;

    config.port = root.get("port", 8080).asInt();
    config.no_banner = root.get("no_banner", false).asBool();
    config.no_clear = root.get("no_clear", false).asBool();
    config.active_api_config = root.get("active_api_config", "migrated-openrouter").asString();
    config.auto_execute_commands_mode = root.get("auto_execute_commands_mode", "ask").asString();
    config.debug = root.get("debug", false).asBool();
}

void ConfigManager::saveConfig(const Config& config) {
    Json::Value root;
    root["port"] = config.port;
    root["no_banner"] = config.no_banner;
    root["no_clear"] = config.no_clear;
    root["active_api_config"] = config.active_api_config;
    root["auto_execute_commands_mode"] = config.auto_execute_commands_mode;
    root["debug"] = config.debug;

    std::filesystem::path p(config_path);
    std::filesystem::create_directories(p.parent_path());

    std::ofstream file(config_path);
    if (file.is_open()) {
        file << root;
    }
}

void ConfigManager::loadExternalConfig(Config& config, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    Json::Value root;
    file >> root;

    config.port = root.get("port", 8080).asInt();
    config.no_banner = root.get("no_banner", false).asBool();
    config.no_clear = root.get("no_clear", false).asBool();
    config.active_api_config = root.get("active_api_config", "migrated-openrouter").asString();
    config.auto_execute_commands_mode = root.get("auto_execute_commands_mode", "ask").asString();
    config.debug = root.get("debug", false).asBool();
}

void ConfigManager::updateConfig(const std::string& key, const std::string& value) {
    Config config;
    loadConfig(config);

    static const std::unordered_map<std::string, std::function<void(Config&, const std::string&)>> updaters = {
        {"port", [](Config& c, const std::string& v){ c.port = std::stoi(v); }},
        {"no_banner", [](Config& c, const std::string& v){ c.no_banner = (v == "true"); }},
        {"no_clear", [](Config& c, const std::string& v){ c.no_clear = (v == "true"); }},
        {"active_api_config", [](Config& c, const std::string& v){ c.active_api_config = v; }},
        {"auto_execute_commands_mode", [](Config& c, const std::string& v){ 
            if (v == "ask" || v == "yes" || v == "no") {
                c.auto_execute_commands_mode = v; 
            } else {
                std::cerr << "Invalid value for auto_execute_commands_mode. Must be 'ask', 'yes', or 'no'." << std::endl;
            }
        }},
        {"debug", [](Config& c, const std::string& v){ c.debug = (v == "true"); }}
    };

    auto it = updaters.find(key);
    if (it != updaters.end()) {
        it->second(config, value);
    }

    saveConfig(config);
}

std::string ConfigManager::getConfigValue(const std::string& key) {
    Config config;
    loadConfig(config);

    static const std::unordered_map<std::string, std::function<std::string(const Config&)>> getters = {
        {"port", [](const Config& c){ return std::to_string(c.port); }},
        {"no_banner", [](const Config& c){ return c.no_banner ? "true" : "false"; }},
        {"no_clear", [](const Config& c){ return c.no_clear ? "true" : "false"; }},
        {"active_api_config", [](const Config& c){ return c.active_api_config; }},
        {"auto_execute_commands_mode", [](const Config& c){ return c.auto_execute_commands_mode; }},
        {"debug", [](const Config& c){ return c.debug ? "true" : "false"; }}
    };

    auto it = getters.find(key);
    if (it != getters.end()) {
        return it->second(config);
    }

    return std::string();
}

std::string ConfigManager::getAllConfig() {
    Config config;
    loadConfig(config);

    Json::Value root;
    root["port"] = config.port;
    root["no_banner"] = config.no_banner;
    root["no_clear"] = config.no_clear;
    root["active_api_config"] = config.active_api_config;
    root["auto_execute_commands_mode"] = config.auto_execute_commands_mode;
    root["debug"] = config.debug;

    Json::StreamWriterBuilder writer;
    return Json::writeString(writer, root);
}
