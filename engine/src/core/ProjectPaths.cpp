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
    
    bool ProjectPaths::IsInitialized() {
        return s_initialized;
    }
    
    std::string ProjectPaths::GetProjectRoot() {
        if (!s_initialized) {
            LOG_ERROR("ProjectPaths not initialized");
            return "";
        }
        return s_projectRoot;
    }
    
    std::string ProjectPaths::GetAssetsPath() {
        return GetProjectRoot() + "/Assets";
    }
    
    std::string ProjectPaths::GetSettingsPath() {
        std::filesystem::path settingsPath = std::filesystem::path(GetProjectDocumentsRoot()) / "ProjectSettings.json";
        EnsureDirectoryExists(settingsPath.parent_path().string());
        return settingsPath.string();
    }

    std::string ProjectPaths::GetScenesPath() {
        return GetProjectRoot() + "/Scenes";
    }

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

    std::string ProjectPaths::GetEditorDocumentsRoot() {
        std::filesystem::path docsRoot = GetDocumentsRoot();
        std::filesystem::path editorDocs = docsRoot / "Grape Engine";
        EnsureDirectoryExists(editorDocs.string());
        return editorDocs.string();
    }

    std::string ProjectPaths::GetLogsPath() {
        std::filesystem::path logsPath = std::filesystem::path(GetProjectDocumentsRoot()) / "logs";
        EnsureDirectoryExists(logsPath.string());
        return logsPath.string();
    }
    
    std::string ProjectPaths::GetCompiledScriptAssemblyPath() {
        return GetTempScriptsPath() + "/GameScripts.dll";
    }
    
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
    
    std::string ProjectPaths::ToAbsolutePath(const std::string& relativePath) {
        std::filesystem::path absolute = std::filesystem::absolute(GetProjectRoot() + "/" + relativePath);
        return absolute.string();
    }
    
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
