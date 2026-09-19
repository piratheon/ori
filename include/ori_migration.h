#ifndef ORI_MIGRATION_H
#define ORI_MIGRATION_H

#include <string>

class MigrationManager {
public:
    MigrationManager(const std::string& config_dir, bool debug_enabled);
    void run();

private:
    std::string config_dir;
    std::string key_file_path;
    std::string config_file_path;
    std::string keys_json_path;
    std::string keys_json_example_path;
    bool debug_enabled;

    // Backfills config keys introduced after the user's config.json was written
    // (rag_memory_enabled, skills_memory_enabled, show_command_output,
    // git_backup_enabled). Idempotent; never overwrites existing values.
    void migrateConfigDefaults();

    bool needsMigration();
    void performMigration();
    bool isExampleModified();
    void renameExample();
    void firstTimeSetup();
};

#endif // ORI_MIGRATION_H
