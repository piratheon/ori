#include "ori_edit.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

// ANSI Color Codes from ori_core.h
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";

bool OriEdit::showPreview(const EditOperation& op) {
    std::cout << BOLD << "Preview of changes for " << op.filename << ":" << RESET << "\n\n";
    
    // Create a temporary file for diff
    std::string tempFile = "/tmp/ori_preview_" + std::to_string(getpid());
    std::ofstream temp(tempFile);
    temp << op.newContent;
    temp.close();
    
    // Show colored diff
    std::string cmd = "diff --color -u " + op.filename + " " + tempFile;
    system(cmd.c_str());
    
    // Cleanup
    unlink(tempFile.c_str());
    return true;
}

bool OriEdit::createBackup(const std::string& filename) {
    std::string backupFile = filename + ".ori.bak";
    std::string cmd = "cp " + filename + " " + backupFile;
    return system(cmd.c_str()) == 0;
}

bool OriEdit::applyChanges(const EditOperation& op) {
    if (op.safe || op.backup) {
        if (!createBackup(op.filename)) {
            std::cerr << RED << "Failed to create backup" << RESET << std::endl;
            return false;
        }
    }
    
    if (op.preview) {
        if (!showPreview(op)) {
            return false;
        }
    }
    
    if (op.interactive) {
        if (!confirmChange("Apply these changes?", "")) {
            return false;
        }
    }

    if (op.diff) {
        std::string tempFile = "/tmp/ori_preview_" + std::to_string(getpid());
        std::ofstream temp(tempFile);
        temp << op.newContent;
        temp.close();
        if (!showDiff(op.filename, tempFile)) {
            unlink(tempFile.c_str());
            return false;
        }
        unlink(tempFile.c_str());
    }
    
    // Write new content
    std::ofstream file(op.filename);
    if (!file) {
        std::cerr << RED << "Failed to open file for writing" << RESET << std::endl;
        return false;
    }
    file << op.newContent;
    file.close();
    
    std::cout << GREEN << "Changes applied successfully" << RESET << std::endl;
    return true;
}

bool isGuiEnvironment();

bool OriEdit::showDiff(const std::string& file1, const std::string& file2) {
    if (isGuiEnvironment() && system("command -v meld > /dev/null") == 0) {
        std::string cmd = "meld " + file1 + " " + file2;
        system(cmd.c_str());
        return true;
    }

    std::string cmd = "diff --color -u " + file1 + " " + file2;
    system(cmd.c_str());

    std::cout << YELLOW << "Apply these changes? (y/n): " << RESET;
    std::string response;
    std::getline(std::cin, response);
    return (response == "y" || response == "Y");
}

bool OriEdit::restoreBackup(const std::string& filename) {
    std::string backupFile = filename + ".ori.bak";
    if (access(backupFile.c_str(), F_OK) != -1) {
        std::string cmd = "mv " + backupFile + " " + filename;
        if (system(cmd.c_str()) == 0) {
            std::cout << GREEN << "Backup restored successfully" << RESET << std::endl;
            return true;
        }
    }
    std::cerr << RED << "No backup file found" << RESET << std::endl;
    return false;
}

bool OriEdit::confirmChange(const std::string& description, const std::string& preview) {
    std::cout << YELLOW << description << " (y/n): " << RESET;
    std::string response; 
    std::getline(std::cin, response);
    return (response == "y" || response == "Y");
}

void OriEdit::openComparisonTerminal(const std::string& file1, const std::string& file2) {
    // Open new terminal with vimdiff
    std::string cmd = "x-terminal-emulator -e vimdiff " + file1 + " " + file2;
    system(cmd.c_str());
}

bool OriEdit::validateOperation(const EditOperation& op) {
    // Check if file exists
    if (access(op.filename.c_str(), F_OK) == -1) {
        std::cerr << RED << "File does not exist: " << op.filename << RESET << std::endl;
        return false;
    }
    
    // Check write permissions
    if (access(op.filename.c_str(), W_OK) == -1) {
        std::cerr << RED << "No write permission for: " << op.filename << RESET << std::endl;
        return false;
    }
    
    // Check version control if safe mode
    if (op.safe && !isVersionControlled(op.filename)) {
        std::cerr << YELLOW << "Warning: File is not under version control" << RESET << std::endl;
        if (!confirmChange("Continue anyway?", "")) {
            return false;
        }
    }
    
    return true;
}

bool OriEdit::checkConflicts(const std::string& filename) {
    // Check if file is being edited by another process
    std::string cmd = "lsof " + filename + " 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool OriEdit::isVersionControlled(const std::string& filename) {
    std::string cmd = "git ls-files --error-unmatch " + filename + " 2>/dev/null";
    return system(cmd.c_str()) == 0;
}