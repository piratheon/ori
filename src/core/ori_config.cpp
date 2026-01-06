#include "ori_core.h"
#include <fstream>
#include <json/json.h>
#include <sys/stat.h>
#include <filesystem>

Config::Config() : port(8080), no_banner(false), no_clear(false), model("google/gemini-2.0-flash-exp:free") {}

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
}

void ConfigManager::saveConfig(const Config& config) {
    Json::Value root;
    root["port"] = config.port;
    root["no_banner"] = config.no_banner;
    root["no_clear"] = config.no_clear;
    root["model"] = config.model;

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
}

void ConfigManager::updateConfig(const std::string& key, const std::string& value) {
    Config config;
    loadConfig(config);

    if (key == "port") {
        config.port = std::stoi(value);
    } else if (key == "no_banner") {
        config.no_banner = (value == "true");
    } else if (key == "no_clear") {
        config.no_clear = (value == "true");
    } else if (key == "model") {
        config.model = value;
    }

    saveConfig(config);
}
