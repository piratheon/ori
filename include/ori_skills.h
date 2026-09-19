#ifndef ORI_SKILLS_H
#define ORI_SKILLS_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_set>
#include <json/json.h>

struct AgentSkill {
    std::string name;
    std::string description;
    std::string instructions;
    bool is_learned = false;
    bool is_builtin = false;
    std::string source_file;
};

class LearnedSkillsMemory {
private:
    std::string storage_path;
    std::vector<AgentSkill> learned_skills;

public:
    LearnedSkillsMemory();
    void load();
    void save();
    bool addSkill(const AgentSkill& skill);
    bool removeSkill(const std::string& name);
    const std::vector<AgentSkill>& getSkills() const { return learned_skills; }
};

struct RAGChunk {
    std::string id;
    std::string content;
    std::string source;
    uint64_t timestamp = 0;

    // Derived data, computed once when the chunk is inserted/loaded (never persisted),
    // so retrieval does not have to re-tokenize the whole memory on every query.
    std::unordered_set<std::string> token_set;
    size_t token_count = 0; // number of tokens including duplicates (used for length normalisation)
};

class RAGMemory {
private:
    std::string storage_path;
    std::vector<RAGChunk> chunks;
    bool enabled = false;
    size_t max_chunks = kDefaultMaxChunks;
    uint64_t next_seq = 1; // monotonic, keeps chunk ids unique even after FIFO eviction

    static void indexChunk(RAGChunk& chunk);
    void enforceCap(); // FIFO eviction: drops the oldest chunks beyond max_chunks

public:
    // Upper bound on stored chunks; the oldest chunks are evicted first.
    static constexpr size_t kDefaultMaxChunks = 500;

    RAGMemory();
    void setMaxChunks(size_t n);
    size_t getMaxChunks() const { return max_chunks; }
    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }
    void load();
    void save();
    void addChunk(const std::string& content, const std::string& source = "general");
    void clear();
    std::vector<RAGChunk> retrieveRelevant(const std::string& query, size_t max_results = 3);
    std::string getPromptContext(const std::string& query);
    const std::vector<RAGChunk>& getChunks() const { return chunks; }
};

class SkillsManager {
private:
    std::vector<AgentSkill> skills;
    LearnedSkillsMemory learnedMemory;

public:
    SkillsManager();
    void discoverAndLoad(bool skills_memory_enabled);
    void registerBuiltinSkills();
    bool addLearnedSkill(const std::string& name, const std::string& desc, const std::string& instructions);
    bool removeLearnedSkill(const std::string& name);
    std::string getSkillsPromptContext() const;
    const std::vector<AgentSkill>& getSkills() const { return skills; }
    LearnedSkillsMemory& getLearnedMemory() { return learnedMemory; }
};

#endif // ORI_SKILLS_H
