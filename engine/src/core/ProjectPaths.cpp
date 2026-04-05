/* Start Header *****************************************************************/
/*!
\file   ProjectPaths.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
Implementation of centralized project path management.
*/
/* End Header *******************************************************************/

#include "core/ProjectPaths.h"

#ifdef _WIN32
#include <ShlObj.h>
#endif

#include "core/Logger.h"
#include <filesystem>
#include <cstdlib>

#ifdef INFO
#undef INFO
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef ERROR
#undef ERROR
#endif

namespace Engine {
    
    // Static member initialization
    std::string ProjectPaths::s_projectRoot = "";
    bool ProjectPaths::s_initialized = false;
    
    /**
     * @brief Initialize project root state and ensure required directories exist.
     * @param projectRoot Absolute or relative project root path.
     */
    void ProjectPaths::Initialize(const std::string& projectRoot) {
        if (s_initialized && s_projectRoot == projectRoot) {
            return;
        }

        s_projectRoot = projectRoot;
        s_initialized = true;
        
        // Ensure all required directories exist
        EnsureDirectoryExists(GetAssetsPath());
        EnsureDirectoryExists(GetScenesPath());
        EnsureDirectoryExists(GetLogsPath());
        EnsureDirectoryExists(GetSavesPath());

        const std::filesystem::path logsPath = GetLogsPath();
        if (!logsPath.empty()) {
            Logger::Get().SetLogFile(LogLevel::INFO, (logsPath / "engine.log").string());
            Logger::Get().SetLogFile(LogLevel::ERROR, (logsPath / "error.log").string());
        }
        
        LOG_INFO("ProjectPaths initialized: " << s_projectRoot);
        LOG_INFO("  Assets: " << GetAssetsPath());
        if (!logsPath.empty()) {
            LOG_INFO("  Logs: " << logsPath.string());
        }
    }
    
    /**
     * @brief Check whether project paths were initialized.
     * @return True when Initialize has completed successfully.
     */
    bool ProjectPaths::IsInitialized() {
        return s_initialized;
    }
    
    /**
     * @brief Get current project root path.
     * @return Project root path or empty string when uninitialized.
     */
    std::string ProjectPaths::GetProjectRoot() {
        if (!s_initialized) {
            LOG_ERROR("ProjectPaths not initialized");
            return "";
        }
        return s_projectRoot;
    }
    
    /**
     * @brief Get the project Assets directory path.
     * @return Full path to the Assets directory.
     */
    std::string ProjectPaths::GetAssetsPath() {
        return GetProjectRoot() + "/Assets";
    }
    
    /**
     * @brief Get the ProjectSettings.json path and ensure parent folder exists.
     * @return Full path to ProjectSettings.json.
     */
    std::string ProjectPaths::GetSettingsPath() {
        std::filesystem::path settingsPath = std::filesystem::path(GetProjectRoot()) / "ProjectSettings.json";
        EnsureDirectoryExists(settingsPath.parent_path().string());
        return settingsPath.string();
    }

    /**
     * @brief Get the project Scenes directory path.
     * @return Full path to the Scenes directory.
     */
    std::string ProjectPaths::GetScenesPath() {
        return GetProjectRoot() + "/Scenes";
    }

    /**
     * @brief Get per-project temporary scripts output directory.
     * @return Full path to temporary scripts directory, or empty string on failure.
     */
    std::string ProjectPaths::GetTempScriptsPath() {
        try {
            // Create a stable temp directory per project
            std::filesystem::path projRoot = GetProjectRoot();
            std::string projName = projRoot.filename().string();
            std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "GrapeEngine" / projName;
            
            // Ensure the directory exists
            if (!std::filesystem::exists(tempDir)) {
                std::filesystem::create_directories(tempDir);
            }
            
            return tempDir.string();
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to get temp scripts path: " << e.what());
            return "";
        }
    }

    /**
     * @brief Resolve the user Documents root folder on the current platform.
     * @return Full path to Documents root, with cwd fallback when unavailable.
     */
    std::string ProjectPaths::GetDocumentsRoot() {
#ifdef _WIN32
        PWSTR widePath = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &widePath);
        if (SUCCEEDED(hr) && widePath) {
            std::filesystem::path docsPath(widePath);
            CoTaskMemFree(widePath);
            return docsPath.string();
        }
        if (widePath) {
            CoTaskMemFree(widePath);
        }
#else
        const char* home = std::getenv("HOME");
        if (home && *home) {
            return (std::filesystem::path(home) / "Documents").string();
        }
#endif
        return std::filesystem::current_path().string();
    }

    /**
     * @brief Get project-specific documents root and ensure it exists.
     * @return Full path to Documents/Grape Engine/<project-name>.
     */
    std::string ProjectPaths::GetProjectDocumentsRoot() {
        std::filesystem::path projectRoot = GetProjectRoot();
        std::string projectName = projectRoot.filename().string();
        if (projectName.empty()) {
            projectName = "UnknownProject";
        }
        std::filesystem::path docsRoot = GetDocumentsRoot();
        std::filesystem::path projectDocs = docsRoot / "Grape Engine" / projectName;
        EnsureDirectoryExists(projectDocs.string());
        return projectDocs.string();
    }

    /**
     * @brief Get editor-wide documents root and ensure it exists.
     * @return Full path to Documents/Grape Engine.
     */
    std::string ProjectPaths::GetEditorDocumentsRoot() {
        std::filesystem::path docsRoot = GetDocumentsRoot();
        std::filesystem::path editorDocs = docsRoot / "Grape Engine";
        EnsureDirectoryExists(editorDocs.string());
        return editorDocs.string();
    }

    /**
     * @brief Get per-project logs directory and ensure it exists.
     * @return Full path to logs directory.
     */
    std::string ProjectPaths::GetLogsPath() {
        std::filesystem::path logsPath = std::filesystem::path(GetProjectDocumentsRoot()) / "logs";
        EnsureDirectoryExists(logsPath.string());
        return logsPath.string();
    }

    /**
     * @brief Get per-project save directory and ensure it exists.
     * @return Full path to saves directory.
     */
    std::string ProjectPaths::GetSavesPath() {
        std::filesystem::path savesPath = std::filesystem::path(GetProjectDocumentsRoot()) / "saves";
        EnsureDirectoryExists(savesPath.string());
        return savesPath.string();
    }
    
    /**
     * @brief Get compiled managed assembly output path.
     * @return Full path to GameScripts.dll output.
     */
    std::string ProjectPaths::GetCompiledScriptAssemblyPath() {
        return GetTempScriptsPath() + "/GameScripts.dll";
    }
    
    /**
     * @brief Get C# project output directory and ensure it exists.
     * @return Full path to csproj directory, or empty string on failure.
     */
    std::string ProjectPaths::GetCsProjPath() {
        try {
            std::filesystem::path csprojDir = GetTempScriptsPath() + "/csproj";
            
            // Ensure the directory exists
            if (!std::filesystem::exists(csprojDir)) {
                std::filesystem::create_directories(csprojDir);
            }
            
            return csprojDir.string();
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to get csproj path: " << e.what());
            return "";
        }
    }
    
    /**
     * @brief Convert a project-relative path into an absolute filesystem path.
     * @param relativePath Path relative to current project root.
     * @return Absolute filesystem path.
     */
    std::string ProjectPaths::ToAbsolutePath(const std::string& relativePath) {
        std::filesystem::path absolute = std::filesystem::absolute(GetProjectRoot() + "/" + relativePath);
        return absolute.string();
    }
    
    /**
     * @brief Convert an absolute filesystem path into project-relative form.
     * @param absolutePath Absolute filesystem path.
     * @return Relative path from project root, or empty string on failure.
     */
    std::string ProjectPaths::ToRelativePath(const std::string& absolutePath) {
        try {
            std::filesystem::path abs = std::filesystem::absolute(absolutePath);
            std::filesystem::path root = std::filesystem::absolute(GetProjectRoot());
            
            std::filesystem::path relative = std::filesystem::relative(abs, root);
            return relative.string();
        }
        catch (const std::exception& e) {
            LOG_WARNING("Failed to convert to relative path: " << e.what());
            return "";
        }
    }
    
    /**
     * @brief Check if a path resolves within the current project root.
     * @param path Filesystem path to evaluate.
     * @return True when the path is contained by the project root.
     */
    bool ProjectPaths::IsInProject(const std::string& path) {
        try {
            std::filesystem::path abs = std::filesystem::absolute(path);
            std::filesystem::path root = std::filesystem::absolute(GetProjectRoot());
            
            // Check if path starts with project root
            auto relativePath = std::filesystem::relative(abs, root);
            return !relativePath.empty() && relativePath.string().substr(0, 2) != "..";
        }
        catch (const std::exception&) {
            return false;
        }
    }
    
    /**
     * @brief Create a directory tree when it does not exist.
     * @param path Directory path to create.
     */
    void ProjectPaths::EnsureDirectoryExists(const std::string& path) {
        try {
            std::filesystem::path dirPath(path);
            if (!std::filesystem::exists(dirPath)) {
                std::filesystem::create_directories(dirPath);
                LOG_INFO("Created directory: " << path);
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to create directory " << path << ": " << e.what());
        }
    }
    
}
