#include "ori_agents.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

AgentsManager::AgentsManager() {}

void AgentsManager::discoverAndLoad() {
    loaded_rules.clear();
    fs::path current_dir = fs::current_path();

    std::vector<fs::path> found_paths;

    fs::path check_dir = current_dir;
    while (true) {
        fs::path agents_path = check_dir / "AGENTS.md";
        if (fs::exists(agents_path) && fs::is_regular_file(agents_path)) {
            found_paths.push_back(agents_path);
        }
        if (check_dir.has_parent_path() && check_dir.parent_path() != check_dir) {
            check_dir = check_dir.parent_path();
        } else {
            break;
        }
    }

    std::reverse(found_paths.begin(), found_paths.end());

    for (const auto& path : found_paths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            loaded_rules.push_back({path.string(), buffer.str()});
        }
    }
}

std::string AgentsManager::getAgentsPromptContext() const {
    if (loaded_rules.empty()) {
        return "";
    }
    std::string context = "\n[AGENTS.MD INSTRUCTIONS]\n";
    context += "The workspace has AGENTS.md instructions that must be strictly followed:\n\n";
    for (const auto& rule : loaded_rules) {
        context += "--- File: " + rule.filepath + " ---\n";
        context += rule.content + "\n\n";
    }
    context += "[END AGENTS.MD INSTRUCTIONS]\n";
    return context;
}
