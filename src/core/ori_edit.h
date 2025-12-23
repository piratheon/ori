#ifndef ORI_EDIT_H
#define ORI_EDIT_H

#include <string>
#include <vector>

class OriEdit {
public:
    struct EditOperation {
        std::string type;        // search, replace, modify, refactor, rename, compare
        std::string filename;    // Target file
        std::string oldContent;  // Original content
        std::string newContent;  // Modified content
        bool preview;            // Show changes before applying
        bool diff;              // Open diff in separate terminal
        bool backup;            // Create backup before changes
        bool interactive;        // Prompt for each change
        bool safe;              // Enable all safety features
    };

    // File operations
    static bool showPreview(const EditOperation& op);
    static bool createBackup(const std::string& filename);
    static bool applyChanges(const EditOperation& op);
    static bool showDiff(const std::string& file1, const std::string& file2);
    static bool restoreBackup(const std::string& filename);
    
    // Interactive operations
    static bool confirmChange(const std::string& description, const std::string& preview);
    static void openComparisonTerminal(const std::string& file1, const std::string& file2);
    
    // Safety checks
    static bool validateOperation(const EditOperation& op);
    static bool checkConflicts(const std::string& filename);
    static bool isVersionControlled(const std::string& filename);
};

#endif // ORI_EDIT_H