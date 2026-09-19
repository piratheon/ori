#include "ori_task.h"
#include "ori_core.h"
#include <iostream>
#include <sstream>
#include <json/json.h>

SubAgent::SubAgent(const std::string& r, const std::string& n, const std::string& desc, const std::string& sys_p)
    : role(r), name(n), description(desc), system_prompt(sys_p) {}

std::string SubAgent::execute(OriAssistant& assistant, const std::string& task_description) {
    std::string full_prompt = "Role: " + name + " (" + role + ")\n" + system_prompt + "\n\nTask: " + task_description;
    return assistant.sendQuery(full_prompt);
}

TaskDispatcher::TaskDispatcher() {
    // Register standard subagents
    registerSubAgent(std::make_shared<SubAgent>(
        "generalist", "General Assistant", "Handles general reasoning and tasks",
        "You are an expert generalist AI assistant. Solve the user's task methodically."
    ));
    registerSubAgent(std::make_shared<SubAgent>(
        "coder", "Software Engineer", "Specializes in writing and refactoring code",
        "You are an expert C++ and systems software engineer. Write clean, idiomatic, robust code."
    ));
    registerSubAgent(std::make_shared<SubAgent>(
        "reviewer", "Code Reviewer", "Specializes in code review and bug detection",
        "You are an expert code reviewer. Analyze code for bugs, edge cases, and performance improvements."
    ));
    registerSubAgent(std::make_shared<SubAgent>(
        "executor", "Command Executor", "Specializes in shell execution and Linux system administration",
        "You are an expert Linux sysadmin. Determine the exact terminal commands required."
    ));
    registerSubAgent(std::make_shared<SubAgent>(
        "planner", "Task Planner", "Breaks down goals into structured subtasks",
        "You are a task planning agent. Break down complex goals into sequential subtasks."
    ));
}

void TaskDispatcher::registerSubAgent(std::shared_ptr<SubAgent> agent) {
    if (agent) {
        subagents[agent->role] = agent;
    }
}

std::shared_ptr<SubAgent> TaskDispatcher::getSubAgent(const std::string& role) {
    auto it = subagents.find(role);
    if (it != subagents.end()) {
        return it->second;
    }
    return subagents["generalist"];
}

int TaskDispatcher::addTask(const std::string& title, const std::string& description, const std::string& role) {
    Task t;
    t.id = next_task_id++;
    t.title = title;
    t.description = description;
    t.status = TaskStatus::PENDING;
    t.subagent_role = role.empty() ? "generalist" : role;
    tasks.push_back(t);
    return t.id;
}

bool TaskDispatcher::decomposeGoal(OriAssistant& assistant, const std::string& goal) {
    std::string prompt = "Decompose the following goal into subtasks.\n"
                         "Goal: " + goal + "\n\n"
                         "Respond ONLY with a valid JSON array of objects, where each object has:\n"
                         "- \"title\": short title\n"
                         "- \"description\": detailed instructions\n"
                         "- \"role\": one of [generalist, coder, reviewer, executor, planner]\n"
                         "Example:\n"
                         "[{\"title\": \"Analyze code\", \"description\": \"Inspect main.cpp\", \"role\": \"coder\"}]";

    std::string response = assistant.sendQuery(prompt);

    // Extract JSON array from response
    size_t start = response.find('[');
    size_t end = response.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        // Fallback: add single task
        addTask("Execute Goal", goal, "generalist");
        return true;
    }

    std::string json_str = response.substr(start, end - start + 1);
    Json::CharReaderBuilder readerBuilder;
    Json::Value root;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());

    if (reader->parse(json_str.c_str(), json_str.c_str() + json_str.length(), &root, &errs) && root.isArray()) {
        for (const auto& item : root) {
            std::string title = item.get("title", "Task").asString();
            std::string desc = item.get("description", "").asString();
            std::string role = item.get("role", "generalist").asString();
            addTask(title, desc, role);
        }
        return true;
    }

    addTask("Execute Goal", goal, "generalist");
    return true;
}

bool TaskDispatcher::runNextTask(OriAssistant& assistant) {
    for (auto& task : tasks) {
        if (task.status == TaskStatus::PENDING) {
            task.status = TaskStatus::IN_PROGRESS;
            std::cout << colorize(CYAN, "[➜] Dispatching Task #" + std::to_string(task.id) + ": " + task.title) << "\n";
            std::cout << colorize(BLUE, "    Subagent: ") << task.subagent_role << "\n";
            std::cout << colorize(BLUE, "    Description: ") << task.description << "\n";

            auto agent = getSubAgent(task.subagent_role);
            task.result = agent->execute(assistant, task.description);
            assistant.handleResponse(task.result, false);

            task.status = TaskStatus::COMPLETED;
            std::cout << colorize(GREEN, "[✓] Task #" + std::to_string(task.id) + " Completed!") << "\n\n";
            return true;
        }
    }
    std::cout << colorize(YELLOW, "[!] No pending tasks found.") << "\n";
    return false;
}

void TaskDispatcher::runAllTasks(OriAssistant& assistant) {
    while (runNextTask(assistant)) {
        // Continue running until no pending tasks remain
    }
}

void TaskDispatcher::displayTasks() const {
    std::cout << BOLD << "--- Task Dispatcher Queue ---" << RESET << "\n";
    if (tasks.empty()) {
        std::cout << "No tasks in queue." << "\n";
        return;
    }

    for (const auto& task : tasks) {
        std::string status_str;
        std::string symbol;
        switch (task.status) {
            case TaskStatus::PENDING:
                status_str = "PENDING";
                symbol = colorize(YELLOW, "[+]");
                break;
            case TaskStatus::IN_PROGRESS:
                status_str = "IN_PROGRESS";
                symbol = colorize(CYAN, "[➜]");
                break;
            case TaskStatus::COMPLETED:
                status_str = "COMPLETED";
                symbol = colorize(GREEN, "[✓]");
                break;
            case TaskStatus::FAILED:
                status_str = "FAILED";
                symbol = colorize(RED, "[x]");
                break;
        }
        std::cout << symbol << " #" << task.id << " [" << status_str << "] (" << task.subagent_role << ") " << BOLD << task.title << RESET << "\n";
        std::cout << "    " << task.description << "\n";
    }
    std::cout << BOLD << "-----------------------------" << RESET << "\n";
}

void TaskDispatcher::clearTasks() {
    tasks.clear();
    next_task_id = 1;
}
