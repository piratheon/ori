#ifndef ORI_EDIT_H
#define ORI_EDIT_H

#include <string>

struct EditOperation {
    std::string type;
    std::string filename;
    std::string newContent;
    bool preview;
    bool diff;
    bool backup;
    bool interactive;
    bool safe;
};

namespace OriEdit {
    bool showPreview(const EditOperation& op);
    bool createBackup(const std::string& filename);
    bool applyChanges(const EditOperation& op);
    bool showDiff(const std::string& file1, const std::string& file2);
    bool restoreBackup(const std::string& filename);
    bool confirmChange(const std::string& description, const std::string& preview);
    void openComparisonTerminal(const std::string& file1, const std::string& file2);
    bool validateOperation(const EditOperation& op);
    bool checkConflicts(const std::string& filename);
    bool isVersionControlled(const std::string& filename);
}

#endif // ORI_EDIT_H