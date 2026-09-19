#include "ori_skills.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <set>
#include <cmath>

namespace fs = std::filesystem;

static std::string getHomeConfigDir() {
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/ori";
    }
    return ".ori";
}

static std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    return lower;
}

static std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            token += std::tolower(static_cast<unsigned char>(c));
        } else if (!token.empty()) {
            if (token.length() > 2) {
                tokens.push_back(token);
            }
            token.clear();
        }
    }
    if (!token.empty() && token.length() > 2) {
        tokens.push_back(token);
    }
    return tokens;
}

// ================= LearnedSkillsMemory =================

LearnedSkillsMemory::LearnedSkillsMemory() {
    storage_path = getHomeConfigDir() + "/learned_skills.json";
}

void LearnedSkillsMemory::load() {
    learned_skills.clear();
    if (!fs::exists(storage_path)) return;

    std::ifstream file(storage_path);
    if (!file.is_open()) return;

    Json::Value root;
    file >> root;
    file.close();

    if (root.isArray()) {
        for (const auto& item : root) {
            AgentSkill skill;
            skill.name = item.get("name", "").asString();
            skill.description = item.get("description", "").asString();
            skill.instructions = item.get("instructions", "").asString();
            skill.is_learned = true;
            skill.source_file = storage_path;
            if (!skill.name.empty()) {
                learned_skills.push_back(skill);
            }
        }
    }
}

void LearnedSkillsMemory::save() {
    Json::Value root(Json::arrayValue);
    for (const auto& skill : learned_skills) {
        Json::Value item;
        item["name"] = skill.name;
        item["description"] = skill.description;
        item["instructions"] = skill.instructions;
        root.append(item);
    }

    fs::create_directories(fs::path(storage_path).parent_path());
    std::ofstream file(storage_path);
    if (file.is_open()) {
        file << root;
        file.close();
    }
}

bool LearnedSkillsMemory::addSkill(const AgentSkill& skill) {
    AgentSkill new_skill = skill;
    new_skill.is_learned = true;
    new_skill.source_file = storage_path;

    for (auto& s : learned_skills) {
        if (s.name == skill.name) {
            s = new_skill;
            save();
            return true;
        }
    }

    learned_skills.push_back(new_skill);
    save();
    return true;
}

bool LearnedSkillsMemory::removeSkill(const std::string& name) {
    auto it = std::remove_if(learned_skills.begin(), learned_skills.end(), [&](const AgentSkill& s) {
        return s.name == name;
    });
    if (it != learned_skills.end()) {
        learned_skills.erase(it, learned_skills.end());
        save();
        return true;
    }
    return false;
}

// ================= RAGMemory =================

RAGMemory::RAGMemory() {
    storage_path = getHomeConfigDir() + "/rag_memory.json";
}

void RAGMemory::indexChunk(RAGChunk& chunk) {
    std::vector<std::string> tokens = tokenize(chunk.content);
    chunk.token_count = tokens.size();
    chunk.token_set.clear();
    chunk.token_set.insert(tokens.begin(), tokens.end());
}

void RAGMemory::enforceCap() {
    if (max_chunks == 0) {
        chunks.clear();
        return;
    }
    if (chunks.size() > max_chunks) {
        // Chunks are stored oldest-first, so FIFO eviction drops from the front.
        chunks.erase(chunks.begin(), chunks.begin() + (chunks.size() - max_chunks));
    }
}

void RAGMemory::setMaxChunks(size_t n) {
    max_chunks = n;
    size_t before = chunks.size();
    enforceCap();
    if (chunks.size() != before) {
        save();
    }
}

void RAGMemory::load() {
    chunks.clear();
    if (!fs::exists(storage_path)) return;

    std::ifstream file(storage_path);
    if (!file.is_open()) return;

    Json::Value root;
    file >> root;
    file.close();

    if (root.isArray()) {
        for (const auto& item : root) {
            RAGChunk chunk;
            chunk.id = item.get("id", "").asString();
            chunk.content = item.get("content", "").asString();
            chunk.source = item.get("source", "general").asString();
            chunk.timestamp = item.get("timestamp", 0).asUInt64();
            if (!chunk.content.empty()) {
                indexChunk(chunk);
                chunks.push_back(std::move(chunk));
            }
        }
    }

    // Files written by older versions may already exceed the cap: trim them once on load.
    size_t before = chunks.size();
    enforceCap();
    next_seq = chunks.size() + 1;
    if (chunks.size() != before) {
        save();
    }
}

void RAGMemory::save() {
    Json::Value root(Json::arrayValue);
    for (const auto& chunk : chunks) {
        Json::Value item;
        item["id"] = chunk.id;
        item["content"] = chunk.content;
        item["source"] = chunk.source;
        item["timestamp"] = (Json::UInt64)chunk.timestamp;
        root.append(item);
    }

    fs::create_directories(fs::path(storage_path).parent_path());
    std::ofstream file(storage_path);
    if (file.is_open()) {
        file << root;
        file.close();
    }
}

void RAGMemory::addChunk(const std::string& content, const std::string& source) {
    if (content.empty()) return;
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    RAGChunk chunk;
    chunk.id = "chunk_" + std::to_string(now) + "_" + std::to_string(next_seq++);
    chunk.content = content;
    chunk.source = source;
    chunk.timestamp = now;
    indexChunk(chunk); // tokenize once, here, instead of on every retrieval

    chunks.push_back(std::move(chunk));
    enforceCap();
    save();
}

void RAGMemory::clear() {
    chunks.clear();
    next_seq = 1;
    save();
}

std::vector<RAGChunk> RAGMemory::retrieveRelevant(const std::string& query, size_t max_results) {
    if (!enabled || chunks.empty() || max_results == 0) return {};

    std::vector<std::string> query_tokens = tokenize(query);
    if (query_tokens.empty()) return {};

    // Score by index so we never copy chunks (and their token sets) just to rank them.
    std::vector<std::pair<double, size_t>> scored;

    for (size_t i = 0; i < chunks.size(); ++i) {
        const RAGChunk& chunk = chunks[i];
        if (chunk.token_count == 0) continue;

        double matches = 0.0;
        for (const auto& qt : query_tokens) {
            if (chunk.token_set.count(qt)) {
                matches += 1.0;
            }
        }

        if (matches > 0) {
            double score = matches / (std::log(chunk.token_count + 1.0) + 1.0);
            scored.push_back({score, i});
        }
    }

    // Highest score first; on ties prefer the more recent chunk (higher index).
    size_t take = std::min(max_results, scored.size());
    std::partial_sort(scored.begin(), scored.begin() + take, scored.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second > b.second;
    });

    std::vector<RAGChunk> results;
    results.reserve(take);
    for (size_t i = 0; i < take; ++i) {
        results.push_back(chunks[scored[i].second]);
    }
    return results;
}

std::string RAGMemory::getPromptContext(const std::string& query) {
    if (!enabled) return "";
    auto relevant = retrieveRelevant(query, 3);
    if (relevant.empty()) return "";

    std::string ctx = "\n[RAG MEMORY RETRIEVED CONTEXT]\n";
    ctx += "The following relevant memory chunks were retrieved from persistent RAG memory:\n";
    for (size_t i = 0; i < relevant.size(); ++i) {
        ctx += "--- Chunk " + std::to_string(i + 1) + " (Source: " + relevant[i].source + ") ---\n";
        ctx += relevant[i].content + "\n";
    }
    ctx += "[END RAG MEMORY CONTEXT]\n";
    return ctx;
}

// ================= SkillsManager =================

SkillsManager::SkillsManager() {}

void SkillsManager::registerBuiltinSkills() {
    AgentSkill git_skill;
    git_skill.name = "git-integration";
    git_skill.description = "Optional pre-edit git snapshots and /undo, only in projects initialized with /init";
    git_skill.instructions = "In a project the user initialized with /init, Ori snapshots the tree before [edit]/[writefile] changes (on a private git ref, never touching their branch or index). /undo restores only the files Ori changed in the last response and drops that exchange from history. Elsewhere there is no snapshot; Ori never runs git init itself.";
    git_skill.is_builtin = true;
    git_skill.source_file = "built-in";
    skills.push_back(git_skill);
}

void SkillsManager::discoverAndLoad(bool skills_memory_enabled) {
    skills.clear();
    registerBuiltinSkills();

    std::vector<fs::path> search_dirs;

    std::string home_skills = getHomeConfigDir() + "/skills";
    if (fs::exists(home_skills) && fs::is_directory(home_skills)) {
        search_dirs.push_back(home_skills);
    }

    fs::path current = fs::current_path();
    if (fs::exists(current / ".ori" / "skills") && fs::is_directory(current / ".ori" / "skills")) {
        search_dirs.push_back(current / ".ori" / "skills");
    }
    if (fs::exists(current / "skills") && fs::is_directory(current / "skills")) {
        search_dirs.push_back(current / "skills");
    }

    for (const auto& dir : search_dirs) {
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                if (ext == ".json") {
                    std::ifstream file(entry.path());
                    if (file.is_open()) {
                        Json::Value root;
                        file >> root;
                        file.close();

                        AgentSkill skill;
                        skill.name = root.get("name", entry.path().stem().string()).asString();
                        skill.description = root.get("description", "").asString();
                        skill.instructions = root.get("instructions", "").asString();
                        skill.source_file = entry.path().string();
                        if (!skill.name.empty()) {
                            skills.push_back(skill);
                        }
                    }
                } else if (ext == ".md") {
                    std::ifstream file(entry.path());
                    if (file.is_open()) {
                        std::string line;
                        std::string desc;
                        std::stringstream body;
                        bool first = true;
                        while (std::getline(file, line)) {
                            if (first && !line.empty()) {
                                desc = line;
                                if (desc.rfind("# ", 0) == 0) desc = desc.substr(2);
                                first = false;
                            } else {
                                body << line << "\n";
                            }
                        }
                        file.close();

                        AgentSkill skill;
                        skill.name = entry.path().stem().string();
                        skill.description = desc;
                        skill.instructions = body.str();
                        skill.source_file = entry.path().string();
                        skills.push_back(skill);
                    }
                }
            }
        } catch (...) {}
    }

    if (skills_memory_enabled) {
        learnedMemory.load();
        for (const auto& learned : learnedMemory.getSkills()) {
            skills.push_back(learned);
        }
    }
}

bool SkillsManager::addLearnedSkill(const std::string& name, const std::string& desc, const std::string& instructions) {
    AgentSkill skill;
    skill.name = name;
    skill.description = desc;
    skill.instructions = instructions;
    skill.is_learned = true;

    bool ok = learnedMemory.addSkill(skill);
    if (ok) {
        // update loaded list
        for (auto& s : skills) {
            if (s.name == name) {
                s = skill;
                return true;
            }
        }
        skills.push_back(skill);
    }
    return ok;
}

bool SkillsManager::removeLearnedSkill(const std::string& name) {
    bool ok = learnedMemory.removeSkill(name);
    if (ok) {
        auto it = std::remove_if(skills.begin(), skills.end(), [&](const AgentSkill& s) {
            return s.name == name;
        });
        if (it != skills.end()) {
            skills.erase(it, skills.end());
        }
    }
    return ok;
}

std::string SkillsManager::getSkillsPromptContext() const {
    if (skills.empty()) return "";

    std::string context = "\n[CONFIGURED & LEARNED AGENT SKILLS]\n";
    context += "The following skills are available to assist you in executing tasks:\n\n";
    for (const auto& skill : skills) {
        context += "--- Skill: " + skill.name + " (" + (skill.is_builtin ? "built-in" : (skill.is_learned ? "learned" : "user-configured")) + ") ---\n";
        context += "Description: " + skill.description + "\n";
        if (!skill.instructions.empty()) {
            context += "Instructions:\n" + skill.instructions + "\n";
        }
        context += "\n";
    }
    context += "[END AGENT SKILLS]\n";
    return context;
}
