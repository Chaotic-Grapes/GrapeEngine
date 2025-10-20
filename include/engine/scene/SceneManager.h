/* Start Header *****************************************************************/
/*!
\file    SceneManager.h
\authors Muhammad Nur Fadzly Bin Zulkifli (50%), Daniel Neo Zuo Feng Kay (50%)
\par     muhammadnurfadzly.b@digipen.edu, k.danielneozuofeng@digipen.edu
\brief   This file contains the declaration and definition of the SceneManager
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
#include <nlohmann/json.hpp>
#include "core/Logger.h"
#include "scene/Scene.h"
#include "serialization/EntitySerializer.h"
#include "serialization/Serializer.h"

using json = nlohmann::json;

namespace Scenes {
    class SceneManager {
    public:
        using ScenePtr = std::unique_ptr<Scene>;

        /**
         * @brief Adds a new scene to the manager and calls its OnLoad() method.
         * @param scene The scene to add.
         * @return The index of the added scene.
         */
        size_t AddScene(ScenePtr scene) {
            scene->OnLoad();
            m_scenes.push_back(std::move(scene));

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

            // If active, exit first
            if (m_active == index) {
                m_scenes[m_active]->OnExit();
                m_active = NPOS;
            }

            // If pending, clear it
            if (m_pendingActive == index) {
                m_pendingActive = NPOS;
            }

            // Unload and erase
            m_scenes[index]->OnUnload();
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
        Scene* GetActive() {
            if (m_active == NPOS)
                return nullptr;

            return m_scenes[m_active].get();
        }

        /**
         * @brief Gets the currently active scene (const version).
         * @return Const pointer to the active scene, or nullptr if none is active.
         */
        const Scene* GetActive() const {
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
                m_scenes[m_active]->_update(Time::DeltaTime());
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
            auto world = scene.GetWorld();

            try {
                json sceneJson;
                sceneJson["Version"] = version;
                sceneJson["SceneName"] = sceneName;
                sceneJson["EntityCount"] = 0;

                json entities = json::array();
                int entityCount = 0;

                world.Each([&](const ECS::Entity entity) {
                    entities.push_back(Serialization::EntitySerializer::SerializeEntity(world, entity));
                    ++entityCount;
                });

                sceneJson["Entities"] = std::move(entities);
                sceneJson["EntityCount"] = entityCount;

                if (!Serialization::Serializer::SaveJson(filename, "scn", sceneJson)) {
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
                if (!Serialization::Serializer::LoadJson(filename, "scn", sceneJson)) {
                    LOG_ERROR("Error: Cannot open file: " << filename);
                    return false;
                }

                world.DestroyAll();

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

            if (m_active != NPOS)
                m_scenes[m_active]->OnExit();

            m_active = toIndex;
            m_scenes[m_active]->OnEnter();
        }

        std::vector<ScenePtr> m_scenes;
        static constexpr size_t NPOS = static_cast<size_t>(-1);
        size_t m_active = NPOS;
        size_t m_pendingActive = NPOS;
    };
}

#endif
