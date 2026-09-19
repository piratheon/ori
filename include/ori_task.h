#ifndef ORI_TASK_H
#define ORI_TASK_H

#include <string>
#include <vector>
#include <memory>
#include <map>

class OriAssistant;

enum class TaskStatus {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED
};

struct Task {
    int id;
    std::string title;
    std::string description;
    TaskStatus status;
    std::string subagent_role;
    std::string result;
};

class SubAgent {
public:
    std::string role;
    std::string name;
    std::string description;
    std::string system_prompt;

    SubAgent(const std::string& r, const std::string& n, const std::string& desc, const std::string& sys_p);
    std::string execute(OriAssistant& assistant, const std::string& task_description);
};

class TaskDispatcher {
private:
    std::vector<Task> tasks;
    std::map<std::string, std::shared_ptr<SubAgent>> subagents;
    int next_task_id = 1;

public:
    TaskDispatcher();
    void registerSubAgent(std::shared_ptr<SubAgent> agent);
    std::shared_ptr<SubAgent> getSubAgent(const std::string& role);

    int addTask(const std::string& title, const std::string& description, const std::string& role = "generalist");
    bool decomposeGoal(OriAssistant& assistant, const std::string& goal);
    bool runNextTask(OriAssistant& assistant);
    void runAllTasks(OriAssistant& assistant);
    void displayTasks() const;
    void clearTasks();
    const std::vector<Task>& getTasks() const { return tasks; }
};

#endif // ORI_TASK_H
