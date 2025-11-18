/* Start Header *****************************************************************/
/*!
\file    SceneManager.h
\authors Muhammad Nur Fadzly Bin Zulkifli (50%), Daniel Neo Zuo Feng Kay (50%)
\par     muhammadnurfadzly.b@digipen.edu, k.danielneozuofeng@digipen.edu
\brief   
This file contains the declaration and definition of the SceneManager
class, responsible for managing multiple scenes in the application. It
allows adding, removing, and switching between scenes, as well as
serializing and deserializing scenes to and from JSON files.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <memory>
#include <vector>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "core/Logger.h"
#include "core/Profiler.h"
#include "ecs/Hierarchy.h"
#include "scene/Scene.h"
#include "scene/SystemRegistry.h"
#include "serialization/EntitySerializer.h"
#include "serialization/Serializer.h"

using json = nlohmann::json;

namespace Scenes {
    class SceneManager {
    public:
        // Default constructor
        SceneManager() = default;
        
        // Delete copy constructor and copy assignment operator (non-copyable due to unique_ptr)
        SceneManager(const SceneManager&) = delete;
        SceneManager& operator=(const SceneManager&) = delete;
        
        // Default move constructor and move assignment operator
        SceneManager(SceneManager&&) = default;
        SceneManager& operator=(SceneManager&&) = default;

        /**
         * @brief Adds a new scene to the manager.
         * @param scene The scene to add.
         * @return The index of the added scene.
         */
        size_t AddScene(Scene* scene) {
            m_scenes.push_back(std::move(std::unique_ptr<Scene>(scene)));

            // If there is no active scene, make the first added scene active next frame
            if (m_active == NPOS && m_pendingActive == NPOS) {
                m_pendingActive = m_scenes.size() - 1;
            }

            return m_scenes.size() - 1;
        }

        /**
         * @brief Removes a scene by index.
         * @param index The index of the scene to remove.
         * @return True if the scene was removed, false if the index was invalid.
         */
        bool RemoveScene(const size_t index) {
            if (index >= m_scenes.size())
                return false;

            // If active, clear it
            if (m_active == index) {
                m_active = NPOS;
            }

            // If pending, clear it
            if (m_pendingActive == index) {
                m_pendingActive = NPOS;
            }

            // Erase
            m_scenes.erase(m_scenes.begin() + index);

            // Fix indices post-erase
            if (m_active != NPOS && m_active > index)
                m_active--;
            if (m_pendingActive != NPOS && m_pendingActive > index)
                m_pendingActive--;

            return true;
        }

        /**
         * @brief Queues a scene to become active on the next Update boundary.
         * @param index The index of the scene to activate.
         */
        void SetActive(const size_t index) {
            if (index < m_scenes.size()) {
                m_pendingActive = index;
            }
        }

        /**
         * @brief Immediately sets the active scene by index, performing necessary transitions.
         * @param index The index of the scene to activate.
         * @note This bypasses the pending mechanism and should be used cautiously.
         * @warning Using this method during scene updates may lead to inconsistent states.
         */
        void SetActiveImmediate(const size_t index) {
            if (index >= m_scenes.size())
                return;
            
            _performTransition(index);
        }

        /**
         * @brief Gets the currently active scene.
         * @return Pointer to the active scene, or nullptr if none is active.
         */
        Scene* GetActive() const {
            if (m_active == NPOS)
                return nullptr;

            return m_scenes[m_active].get();
        }

        /** @brief Gets the index of the currently active scene. 
         * @return Index of the active scene, or size_t(-1) if none is active.
         */
        size_t GetActiveIndex() const  { return m_active; }

        /** @brief Gets the index of the pending active scene.
         * @return Index of the pending active scene, or size_t(-1) if none is pending.
         */
        size_t GetPendingIndex() const { return m_pendingActive; }

        /** @brief Gets the total number of scenes managed.
         * @return Number of scenes.
         */
        size_t GetSceneCount() const   { return m_scenes.size(); }

        /**
         * @brief Gets a scene by index.
         * @param index The index of the scene to retrieve.
         * @return Pointer to the scene, or nullptr if the index is invalid.
         */
        const Scene* GetScene(const size_t index) const {
            if (index >= m_scenes.size())
                return nullptr;

            return m_scenes[index].get();
        }

        /** @brief Updates the scene manager and the active scene.
         * This processes any pending scene transitions and updates the currently active scene.
         */
        void Update() {
            _processPending();
            if (m_active != NPOS) {
                _updateScene(*m_scenes[m_active], Time::DeltaTime());
            }
        }

		// ************************************** SCENE SERIALIZATION ************************************** //

        /**
         * @brief Saves the specified scene to a JSON file.
         * @param index The index of the scene to save.
         * @param filename Path to the output JSON file.
         * @param sceneName Optional scene name for metadata.
         * @param version Optional version string for metadata.
         * @return true if save was successful, false otherwise.
         */
        bool SaveScene(const size_t index, const std::string& filename,
                       const std::string& sceneName = "Scene",
                       const std::string& version = "1.0") const {
            if (index >= m_scenes.size() || !m_scenes[index])
                return false;

            const Scene& scene = *m_scenes[index];
            const auto& world = scene.GetWorld();

            try {
                json sceneJson;
                sceneJson["Version"] = version;
                sceneJson["SceneName"] = sceneName;
                sceneJson["EntityCount"] = 0;

                // Serialize SystemProfile
                sceneJson["SystemProfile"] = scene.GetSystemProfile();

                json entities = json::array();
                int entityCount = 0;

                world.Each([&](const ECS::Entity entity) {
                    entities.push_back(Serialization::EntitySerializer::SerializeEntity(world, entity));
                    ++entityCount;
                });

                sceneJson["Entities"] = std::move(entities);
                sceneJson["EntityCount"] = entityCount;

                const std::string ext = Serialization::Serializer::HasExtension(filename, "scene") ? "scene" : "scn";
                if (!Serialization::Serializer::SaveJson(filename, ext, sceneJson)) {
                    LOG_ERROR("Error: Could not open file for writing: " << filename);
                    return false;
                }

                LOG_DEBUG("Scene successfully saved to: " << filename);
                LOG_DEBUG(" Entities: " << entityCount);
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("Error saving scene: " << e.what());
                return false;
            }
        }

        /**
         * @brief Loads a scene from a JSON file into the specified scene slot.
         *        This will destroy all existing entities in the scene before loading.
         * @param index The index of the scene to load into.
         * @param filename Path to the input JSON file.
         * @return true if load was successful, false otherwise.
         */
        bool LoadScene(const size_t index, const std::string& filename) const {
            if (index >= m_scenes.size() || !m_scenes[index])
                return false;

            Scene& scene = *m_scenes[index];
            auto& world = scene.GetWorld();

            try {
                json sceneJson;
                const std::string ext = Serialization::Serializer::HasExtension(filename, "scene") ? "scene" : "scn";
                if (!Serialization::Serializer::LoadJson(filename, ext, sceneJson)) {
                    LOG_ERROR("Error: Cannot open file: " << filename);
                    return false;
                }

                world.DestroyAll();

                // Load SystemProfile if present
                if (sceneJson.contains("SystemProfile")) {
                    scene.GetSystemProfile() = sceneJson["SystemProfile"].get<SystemProfile>();
                }

                int loadedCount = 0;
                if (sceneJson.contains("Entities")) {
                    for (const auto& entityJson : sceneJson["Entities"]) {
                        Serialization::EntitySerializer::DeserializeEntity(world, entityJson);
                        ++loadedCount;
                    }
                }
                LOG_DEBUG("Scene successfully loaded: "
                    << sceneJson.value("SceneName", "Unknown") << '\n'
                    << "\tVersion: " << sceneJson.value("Version", "Unknown") << '\n'
					<< "\tEntities loaded: " << loadedCount);

                return true;
            }
            catch (const json::parse_error& e) {
                LOG_ERROR("JSON parse error: " << e.what());
                return false;
            }
            catch (const std::exception& e) {
                LOG_ERROR("Error loading scene: " << e.what());
                return false;
            }
        }

    private:
        void _processPending() {
            if (m_pendingActive == NPOS)
                return;

            _performTransition(m_pendingActive);
            m_pendingActive = NPOS;
        }

        void _performTransition(const size_t toIndex) {
            if (toIndex >= m_scenes.size() || m_active == toIndex)
                return;

            m_active = toIndex;
        }

        /**
         * @brief Updates a scene by executing its system profile.
         * @param scene The scene to update.
         * @param dt Delta time in seconds.
         */
        void _updateScene(Scene& scene, const float dt) {
            auto& world = scene.GetWorld();
            const auto& profile = scene.GetSystemProfile();

            // Execute systems in order based on SystemProfile
            for (const auto& entry : profile.Systems) {
                if (!entry.Enabled)
                    continue;

                // Physics runs on a fixed timestep in Application; skip here to avoid double updates
                if (entry.Name == "Physics")
                    continue;

                auto* systemFunc = SystemRegistry::Get(entry.Name);
                if (systemFunc) {
                    Profiler::Get().BeginScope(entry.Name.c_str());
                    (*systemFunc)(world, dt);
                    Profiler::Get().EndScope(entry.Name.c_str());
                }
                else {
                    // System not found in registry - log warning once per update
                    static std::unordered_set<std::string> s_warnedSystems;
                    if (s_warnedSystems.find(entry.Name) == s_warnedSystems.end()) {
                        LOG_WARNING("System '" << entry.Name << "' not found in SystemRegistry");
                        s_warnedSystems.insert(entry.Name);
                    }
                }
            }

            // Always update transform hierarchy after all systems
            ECS::Hierarchy::UpdateTransforms(world);
        }

        std::vector<std::unique_ptr<Scene>> m_scenes;
        static constexpr size_t NPOS = static_cast<size_t>(-1);
        size_t m_active = NPOS;
        size_t m_pendingActive = NPOS;
    };
}

#endif
