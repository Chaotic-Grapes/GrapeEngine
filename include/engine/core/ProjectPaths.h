/* Start Header *****************************************************************/
/*!
\file   ProjectPaths.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
Centralized project path management for the game project structure.

TODO: This is a temporary solution until the editor is separated from the engine.
      When the engine/editor split happens, this should be replaced with a proper
      project management system that can load projects from any location.
*/
/* End Header *******************************************************************/

#ifndef PROJECTPATHS_H
#define PROJECTPATHS_H

#include <string>
#include <filesystem>

namespace Engine {
    
    /**
     * @brief Manages paths to game project directories and files
     * 
     * TODO: Remove hardcoded "EchoesBelow" path when editor is separated.
     *       Future implementation should support loading projects from any location.
     */
    class ProjectPaths {
    public:
        /**
         * @brief Initialize project paths system
         * @param projectRoot Root directory of the project (optional, defaults to "EchoesBelow")
         * 
         * TODO: Make projectRoot mandatory when editor is separated and can open any project
         */
        static void Initialize(const std::string& projectRoot = "EchoesBelow");
        
        /**
         * @brief Check if project paths have been initialized
         */
        static bool IsInitialized();
        
        /**
         * @brief Get the root directory of the game project
         * TODO: Remove default "EchoesBelow" when editor separation is complete
         */
        static std::string GetProjectRoot();
        
        /**
         * @brief Get the Assets directory path
         * @return Full path to EchoesBelow/Assets/
         */
        static std::string GetAssetsPath();
        
        /**
         * @brief Get the Scenes directory path
         * @return Full path to EchoesBelow/Assets/Scenes/
         */
        static std::string GetScenesPath();
        
        /**
         * @brief Get the Prefabs directory path
         * @return Full path to EchoesBelow/Assets/Prefabs/
         */
        static std::string GetPrefabsPath();
        
        /**
         * @brief Get the Textures directory path
         * @return Full path to EchoesBelow/Assets/Textures/
         */
        static std::string GetTexturesPath();
        
        /**
         * @brief Get the Audio directory path
         * @return Full path to EchoesBelow/Assets/Audio/
         */
        static std::string GetAudioPath();
        
        /**
         * @brief Get the Shaders directory path
         * @return Full path to EchoesBelow/Assets/Shaders/
         */
        static std::string GetShadersPath();
        
        /**
         * @brief Get the Scripts directory path
         * @return Full path to EchoesBelow/Scripts/
         */
        static std::string GetScriptsPath();
        
        /**
         * @brief Get the ProjectSettings.json file path
         * @return Full path to EchoesBelow/ProjectSettings.json
         */
        static std::string GetSettingsPath();
        
        /**
         * @brief Convert a relative project path to absolute path
         * @param relativePath Path relative to project root (e.g., "Assets/Scenes/Main.scn")
         * @return Absolute path
         */
        static std::string ToAbsolutePath(const std::string& relativePath);
        
        /**
         * @brief Convert an absolute path to project-relative path
         * @param absolutePath Absolute file path
         * @return Path relative to project root, or empty if not in project
         */
        static std::string ToRelativePath(const std::string& absolutePath);
        
        /**
         * @brief Check if a path is within the project directory
         */
        static bool IsInProject(const std::string& path);
        
    private:
        static std::string s_projectRoot;
        static bool s_initialized;
        
        // Helper to ensure path exists
        static void EnsureDirectoryExists(const std::string& path);
    };
    
}

#endif
