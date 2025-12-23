#ifndef ORPM_H
#define ORPM_H

#include <string>
#include <vector>
#include <map>
#include <memory>

struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string developer;
    std::string git_url;
    std::string download_url;
    std::string installed_version;
    bool is_installed;
};

class Orpm {
private:
    std::map<std::string, PluginInfo> available_plugins;
    std::string user_plugin_dir;
    std::string system_plugin_dir;
    
public:
    Orpm();
    ~Orpm();
    
    bool initialize();
    void loadPluginDatabase();
    void savePluginDatabase();
    
    // Plugin management functions
    std::vector<PluginInfo> listAvailablePlugins();
    std::vector<PluginInfo> listInstalledPlugins();
    bool installPlugin(const std::string& plugin_name);
    bool removePlugin(const std::string& plugin_name);
    bool updatePlugin(const std::string& plugin_name);
    bool searchPlugins(const std::string& query);
    
    // Utility functions
    PluginInfo getPluginInfo(const std::string& plugin_name);
    bool isPluginInstalled(const std::string& plugin_name);
    std::string getPluginPath(const std::string& plugin_name);
};

#endif // ORPM_H
