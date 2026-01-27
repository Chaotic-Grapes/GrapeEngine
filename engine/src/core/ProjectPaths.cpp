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
#include "core/Logger.h"
#include <filesystem>

namespace Engine {
    
    // Static member initialization
    std::string ProjectPaths::s_projectRoot = "";
    bool ProjectPaths::s_initialized = false;
    
    void ProjectPaths::Initialize(const std::string& projectRoot) {
        s_projectRoot = projectRoot;
        s_initialized = true;
        
        // Ensure all required directories exist
        EnsureDirectoryExists(GetAssetsPath());
        
        LOG_INFO("ProjectPaths initialized: " << s_projectRoot);
        LOG_INFO("  Assets: " << GetAssetsPath());
    }
    
    bool ProjectPaths::IsInitialized() {
        return s_initialized;
    }
    
    std::string ProjectPaths::GetProjectRoot() {
        if (!s_initialized) {
            // TODO: Remove this fallback when editor is separated, should error instead
            LOG_WARNING("ProjectPaths not initialized, using default 'EchoesBelow'");
            return "EchoesBelow";
        }
        return s_projectRoot;
    }
    
    std::string ProjectPaths::GetAssetsPath() {
        return GetProjectRoot() + "/Assets";
    }
    
    std::string ProjectPaths::GetSettingsPath() {
        return GetProjectRoot() + "/ProjectSettings.json";
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
