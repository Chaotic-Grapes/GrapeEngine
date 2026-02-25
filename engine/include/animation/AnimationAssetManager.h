/* Start Header *****************************************************************/
/*!
\file   AnimationAssetManager.h
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Defines helper utilities for loading animation clip/controller assets and
creating transient controllers from legacy components.
*/
/* End Header *******************************************************************/

#pragma once

#include <string>
#include <unordered_map>
#include "Export.h"
#include "animation/AnimationClip2D.h"
#include "animation/AnimationController2D.h"

namespace Animation {
    /**
     * @brief The AnimationAssetManager class provides utilities for loading and managing animation clip and controller assets. 
     * It allows for retrieving existing assets by path, creating transient assets from data, and loading assets from JSON files. 
     * This manager maintains internal mappings of asset IDs to their corresponding data, as well as path-to-ID mappings for efficient retrieval.
     */
    class GRAPEENGINE_API AnimationAssetManager {
    public:
        /**
         * @brief Retrieves the singleton instance of the AnimationAssetManager.
         * @return A reference to the AnimationAssetManager instance.
         */
        static AnimationAssetManager& Get();

        /**
         * @brief Retrieves the ID of an existing animation clip asset by its file path, or loads it if it doesn't exist.
         * @param path The file path of the animation clip asset.
         * @return The unique ID of the animation clip asset.
         */
        uint32_t GetOrLoadClip(const std::string& path);
        
        /**
         * @brief Retrieves the ID of an existing animation controller asset by its file path, or loads it if it doesn't exist.
         * @param path The file path of the animation controller asset.
         * @return The unique ID of the animation controller asset.
         */
        uint32_t GetOrLoadController(const std::string& path);

        /**
         * @brief Finds an animation clip asset by its unique ID.
         * @param id The unique ID of the animation clip asset.
         * @return A pointer to the AnimationClip2DData if found, or nullptr if not found.
         */
        const AnimationClip2DData* FindClip(uint32_t id) const;
        
        /**
         * @brief Finds an animation controller asset by its unique ID.
         * @param id The unique ID of the animation controller asset.
         * @return A pointer to the AnimationController2DData if found, or nullptr if not found.
         */
        const AnimationController2DData* FindController(uint32_t id) const;

        /**
         * @brief Creates a transient animation clip asset from the given clip data.
         * @param clip The AnimationClip2DData to create the transient asset from.
         * @return The unique ID of the created transient animation clip asset.
         */
        uint32_t CreateTransientClip(const AnimationClip2DData& clip);
        
        /**
         * @brief Creates a transient animation controller asset from the given controller data.
         * @param controller The AnimationController2DData to create the transient asset from.
         * @return The unique ID of the created transient animation controller asset.
         */
        uint32_t CreateTransientController(const AnimationController2DData& controller);
        
        /**
         * @brief Creates a transient animation clip asset from the given clip data and associates it with the specified file path.
         * @param path The file path to associate with the transient animation clip asset.
         * @param clip The AnimationClip2DData to create the transient asset from.
         * @return The unique ID of the created transient animation clip asset.
         */
        uint32_t CreateTransientClipWithPath(const std::string& path, const AnimationClip2DData& clip);
        
        /**
         * @brief Creates a transient animation controller asset from the given controller data and associates it with the specified file path.
         * @param path The file path to associate with the transient animation controller asset.
         * @param controller The AnimationController2DData to create the transient asset from.
         * @return The unique ID of the created transient animation controller asset.
         */
        uint32_t CreateTransientControllerWithPath(const std::string& path, const AnimationController2DData& controller);

        /**
         * @brief Loads an animation clip asset from a JSON file at the specified path and outputs the clip data.
         * @param path The file path of the JSON file containing the animation clip data.
         * @param outClip A reference to an AnimationClip2DData structure to store the loaded clip data.
         * @return True if the clip was successfully loaded from the JSON file, false otherwise.
         */
        static bool LoadClipFromJson(const std::string& path, AnimationClip2DData& outClip);
        
        /**
         * @brief Loads an animation controller asset from a JSON file at the specified path and outputs the controller data.
         * @param path The file path of the JSON file containing the animation controller data.
         * @param outController A reference to an AnimationController2DData structure to store the loaded controller data.
         * @return True if the controller was successfully loaded from the JSON file, false otherwise.
         */
        static bool LoadControllerFromJson(const std::string& path, AnimationController2DData& outController);

        /**
         * @brief Computes a hash value for the given file path string, which can be used as a unique identifier for assets.
         * @param path The file path string to hash.
         * @return A 32-bit unsigned integer representing the hash of the file path.
         */
        static uint32_t HashPath(const std::string& path);

    private:
        std::unordered_map<uint32_t, AnimationClip2DData> m_clips;              // Maps clip asset IDs to their corresponding data
        std::unordered_map<uint32_t, AnimationController2DData> m_controllers;  // Maps controller asset IDs to their corresponding data
        std::unordered_map<std::string, uint32_t> m_clipPathToId;               // Maps clip asset file paths to their corresponding IDs for quick lookup
        std::unordered_map<std::string, uint32_t> m_controllerPathToId;         // Maps controller asset file paths to their corresponding IDs for quick lookup
    };
}
