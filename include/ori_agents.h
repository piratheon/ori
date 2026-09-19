#ifndef ORI_AGENTS_H
#define ORI_AGENTS_H

#include <string>
#include <vector>

struct AgentRule {
    std::string filepath;
    std::string content;
};

class AgentsManager {
private:
    std::vector<AgentRule> loaded_rules;

public:
    AgentsManager();
    void discoverAndLoad();
    std::string getAgentsPromptContext() const;
    const std::vector<AgentRule>& getLoadedRules() const { return loaded_rules; }
    bool hasRules() const { return !loaded_rules.empty(); }
};

#endif // ORI_AGENTS_H
