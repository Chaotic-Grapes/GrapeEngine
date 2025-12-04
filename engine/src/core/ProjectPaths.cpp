/* Start Header *****************************************************************/
/*!
\file   ProjectPaths.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
Implementation of centralized project path management.

TODO: This is a temporary solution until the editor is separated from the engine.
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
        // TODO: When editor is separated, validate that projectRoot exists and contains ProjectSettings.json
        s_projectRoot = projectRoot;
        s_initialized = true;
        
        // TODO: This is temporary - remove when editor is separated
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
            // TODO: Remove this fallback when editor is separated - should error instead
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
