#include "ori_core.h"
#include <fstream>
#include <json/json.h>
#include <sys/stat.h>
#include <filesystem>
#include <functional>
#include <unordered_map>

Config::Config() : port(8080), no_banner(false), no_clear(false), model("google/gemini-2.0-flash-exp:free"), debug(false) {}

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

    config.port = root.get("port", 8448).asInt();
    config.no_banner = root.get("no_banner", false).asBool();
    config.no_clear = root.get("no_clear", true).asBool();
    config.model = root.get("model", "qwen/qwen3-coder:free").asString();
    config.debug = root.get("debug", false).asBool();
}

void ConfigManager::saveConfig(const Config& config) {
    Json::Value root;
    root["port"] = config.port;
    root["no_banner"] = config.no_banner;
    root["no_clear"] = config.no_clear;
    root["model"] = config.model;
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
    config.model = root.get("model", "google/gemini-2.0-flash-exp:free").asString();
    config.debug = root.get("debug", false).asBool();
}

void ConfigManager::updateConfig(const std::string& key, const std::string& value) {
    Config config;
    loadConfig(config);

    static const std::unordered_map<std::string, std::function<void(Config&, const std::string&)>> updaters = {
        {"port", [](Config& c, const std::string& v){ c.port = std::stoi(v); }},
        {"no_banner", [](Config& c, const std::string& v){ c.no_banner = (v == "true"); }},
        {"no_clear", [](Config& c, const std::string& v){ c.no_clear = (v == "true"); }},
        {"model", [](Config& c, const std::string& v){ c.model = v; }},
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
        {"model", [](const Config& c){ return c.model; }},
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
    root["model"] = config.model;
    root["debug"] = config.debug;

    Json::StreamWriterBuilder writer;
    return Json::writeString(writer, root);
}
