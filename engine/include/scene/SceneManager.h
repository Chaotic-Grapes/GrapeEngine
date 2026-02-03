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
#include <sstream>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/ProjectPaths.h"
#include "services/TimeSystem.h"
#include "scene/Scene.h"
#include "serialization/EntitySerializer.h"
#include "serialization/Serializer.h"
#include "ecs/PrefabManager.h"
#include "ecs/systems/AudioSystem.h"
#include "core/messaging/MessageSystem.h"
#include "services/ResourceManager.h"

using json = nlohmann::json;

namespace Scenes {
    /**
     * @brief Defines how audio should behave during scene transitions
     */
    enum class SceneTransitionMode {
        Immediate,      // Instant switch, stop all audio immediately
        FadeOut,        // Fade out current scene audio, then switch
        CrossFade       // Overlap: fade out old scene while fading in new scene
    };

    class SceneManager {
    public:
        // Default constructor
        SceneManager() : m_prefabManager(std::make_unique<ECS::PrefabManager>()) { }
        
        // Delete copy constructor and copy assignment operator (non-copyable due to unique_ptr)
        SceneManager(const SceneManager&) = delete;
        SceneManager& operator=(const SceneManager&) = delete;
        
        // Default move constructor and move assignment operator
        SceneManager(SceneManager&&) = default;
        SceneManager& operator=(SceneManager&&) = default;

        /**
         * @brief Gets the prefab manager for this scene manager.
         * @return Pointer to the PrefabManager.
         */
        ECS::PrefabManager* GetPrefabManager() {
            return m_prefabManager.get();
        }

        /**
         * @brief Gets the prefab manager for this scene manager (const version).
         * @return Const pointer to the PrefabManager.
         */
        const ECS::PrefabManager* GetPrefabManager() const {
            return m_prefabManager.get();
        }

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

            // Get owner tag for resource unloading
            const Scene* removedScene = m_scenes[index].get();
            const std::string removedTag = _makeOwnerTag(removedScene);

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

            // Unload resources owned by this scene
            if (!removedTag.empty()) {
                RM.UnloadAssetsByOwner(removedTag);
            }

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
         * @brief Sets active scene with audio fade support
         * @param index The index of the scene to activate
         * @param mode Transition mode (Immediate, FadeOut, or CrossFade)
         * @param fadeDuration Duration of fade in seconds (used for FadeOut and CrossFade modes)
         * @note This queues the transition for the next Update boundary
         */
        void SetActiveWithTransition(const size_t index, SceneTransitionMode mode = SceneTransitionMode::Immediate, float fadeDuration = 1.0f) {
            if (index >= m_scenes.size())
                return;
            if (index == m_active) {
                LOG_WARNING("SceneManager: Ignoring transition to already active scene (index=" << index << ")");
                return;
            }

            m_pendingActive = index;
            m_transitionMode = mode;
            m_transitionFadeDuration = fadeDuration;
        }

        /**
         * @brief Sets active scene with audio fade support by path (loads scene if needed).
         * @param scenePath Path to the scene (absolute or project-relative).
         * @param mode Transition mode (Immediate, FadeOut, or CrossFade).
         * @param fadeDuration Duration of fade in seconds (used for FadeOut and CrossFade modes).
         * @return true if the scene was found/loaded and a transition was queued.
         */
        bool SetActiveWithTransitionByPath(const std::string& scenePath, SceneTransitionMode mode = SceneTransitionMode::Immediate, float fadeDuration = 1.0f) {
            if (scenePath.empty()) {
                return false;
            }

            const std::string normalizedTarget = _normalizePath(_resolveScenePath(scenePath));
            if (normalizedTarget.empty()) {
                return false;
            }

            const size_t existingIndex = _findSceneIndexByPath(normalizedTarget);
            if (existingIndex != NPOS) {
                SetActiveWithTransition(existingIndex, mode, fadeDuration);
                return true;
            }

            auto* scene = new Scene();
            size_t sceneIndex = AddScene(scene);
            if (!LoadScene(sceneIndex, normalizedTarget)) {
                LOG_WARNING("SceneManager: Failed to load scene for transition: " << normalizedTarget);
                RemoveScene(sceneIndex);
                return false;
            }

            SetActiveWithTransition(sceneIndex, mode, fadeDuration);
            return true;
        }

        /**
         * @brief Sets the AudioSystem reference for scene transition coordination
         * @param audioSystem Pointer to the AudioSystem (can be nullptr)
         */
        void SetAudioSystem(ECS::AudioSystem* audioSystem) {
            m_audioSystem = audioSystem;
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
        }

		// ************************************** SCENE SERIALIZATION ************************************** //

        /**
         * @brief Saves the specified scene to a JSON file.
         * @param index The index of the scene to save.
         * @param filename Path to the output JSON file.
         * @param sceneName Optional scene name for metadata.
         * @param version Optional version string for metadata.
         * @param entityOrder Optional ordered list of entity IDs for preserving hierarchy order.
         * @return true if save was successful, false otherwise.
         */
        bool SaveScene(const size_t index, const std::string& filename,
                       const std::string& sceneName = "Scene",
                       const std::string& version = "1.0",
                       const std::vector<uint32_t>* entityOrder = nullptr) const {
            if (index >= m_scenes.size() || !m_scenes[index])
                return false;

            const Scene& scene = *m_scenes[index];
            const auto& world = scene.GetWorld();

            try {
                json sceneJson;
                sceneJson["Version"] = version;
                sceneJson["SceneName"] = sceneName;
                sceneJson["EntityCount"] = 0;

                // Serialize layer manager state (names, masks, visibility, locks, runtime flags)
                json layersJson = json::array();
                const auto layerList = scene.GetLayers().ListLayers();
                for (const auto& p : layerList) {
                    uint16_t id = p.first;
                    const std::string& name = p.second;
                    const auto& layerData = scene.GetLayers().Get(id);
                    json entry;
                    entry["id"] = id;
                    entry["name"] = name;
                    entry["mask"] = scene.GetLayers().GetLayerMask(id);
                    entry["visible"] = scene.GetLayers().IsVisible(id);
                    entry["locked"] = scene.GetLayers().IsLocked(id);
                    entry["renderEnabled"] = layerData.renderEnabled;
                    entry["updateEnabled"] = layerData.updateEnabled;
                    entry["physicsEnabled"] = layerData.physicsEnabled;
                    layersJson.push_back(entry);
                }
                sceneJson["Layers"] = std::move(layersJson);

                json entities = json::array();
                json hierarchyArray = json::array();
                int entityCount = 0;

                // Map to track entity index in save order
                std::unordered_map<uint32_t, size_t> entityToSaveIndex;

                // Use provided entity order if available, otherwise iterate naturally
                if (entityOrder && !entityOrder->empty()) {
                    for (uint32_t entityId : *entityOrder) {
                        ECS::Entity entity = world.Resolve(entityId);
                        if (world.IsAlive(entity)) {
                            entityToSaveIndex[entity.Index] = entityCount;
                            entities.push_back(Serialization::EntitySerializer::SerializeEntity(world, entity));
                            ++entityCount;
                        }
                    }
                } else {
                    world.Each([&](const ECS::Entity entity) {
                        entityToSaveIndex[entity.Index] = entityCount;
                        entities.push_back(Serialization::EntitySerializer::SerializeEntity(world, entity));
                        ++entityCount;
                    });
                }

                // Save hierarchy relationships separately (as child->parent index mappings)
                world.Each([&](const ECS::Entity entity) {
                    ECS::Entity parent = world.ParentOf(entity);
                    if (!parent.IsNull()) {
                        auto childIt = entityToSaveIndex.find(entity.Index);
                        auto parentIt = entityToSaveIndex.find(parent.Index);
                        
                        if (childIt != entityToSaveIndex.end() && parentIt != entityToSaveIndex.end()) {
                            json hierarchyEntry;
                            hierarchyEntry["child"] = childIt->second;
                            hierarchyEntry["parent"] = parentIt->second;
                            hierarchyArray.push_back(hierarchyEntry);
                        }
                    }
                });

                sceneJson["Entities"] = std::move(entities);
                sceneJson["EntityCount"] = entityCount;
                sceneJson["Hierarchy"] = std::move(hierarchyArray);

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
         * @param outEntityOrder Optional output vector to receive the loaded entity order.
         * @return true if load was successful, false otherwise.
         */
        bool LoadScene(const size_t index, const std::string& filename, std::vector<uint32_t>* outEntityOrder = nullptr) {
            if (index >= m_scenes.size() || !m_scenes[index])
                return false;
            
            // Get reference to scene and world
            Scene& scene = *m_scenes[index];
            auto& world = scene.GetWorld();
            
            // Get owner tag scope for resource tracking
            // Explanation: When loading a scene, we want to track all resources
            // loaded for that scene so they can be unloaded when the scene is removed.
            const std::string ownerTag = _makeOwnerTag(&scene);
            struct OwnerScope { // RAII helper for setting/clearing owner tag
                explicit OwnerScope(const std::string& tag) {
                    if (!tag.empty()) {
                        RM.SetOwnerTag(tag); // Set owner tag
                    }
                }
                ~OwnerScope() {
                    RM.ClearOwnerTag(); // Clear owner tag on destruction
                }
            } ownerScope(ownerTag); // Set owner tag for duration of load

            try {
                json sceneJson;
                const std::string ext = Serialization::Serializer::HasExtension(filename, "scene") ? "scene" : "scn";
                if (!Serialization::Serializer::LoadJson(filename, ext, sceneJson)) {
                    LOG_ERROR("Error: Cannot open file: " << filename);
                    return false;
                }

                // Restore layer definitions (names, masks, visibility, locks, runtime flags) before entity deserialization
                if (sceneJson.contains("Layers") && sceneJson["Layers"].is_array()) {
                    const auto& layersArr = sceneJson["Layers"];
                    for (const auto& ent : layersArr) {
                        if (!ent.contains("id") || !ent.contains("name")) continue;
                        const uint16_t lid = ent["id"].get<uint16_t>();
                        const std::string lname = ent["name"].get<std::string>();
                        // Create at slot if missing; rename existing otherwise
                        if (!scene.GetLayers().CreateLayerAt(lid, lname)) {
                            scene.GetLayers().RenameLayer(lid, lname);
                        }
                        if (ent.contains("mask")) {
                            uint32_t mask = ent["mask"].get<uint32_t>();
                            scene.GetLayers().SetLayerMask(lid, mask);
                        }
                        if (ent.contains("visible")) scene.GetLayers().SetVisibility(lid, ent["visible"].get<bool>());
                        if (ent.contains("locked")) scene.GetLayers().SetLocked(lid, ent["locked"].get<bool>());
                        if (ent.contains("renderEnabled")) scene.GetLayers().SetRenderEnabled(lid, ent["renderEnabled"].get<bool>());
                        if (ent.contains("updateEnabled")) scene.GetLayers().SetUpdateEnabled(lid, ent["updateEnabled"].get<bool>());
                        if (ent.contains("physicsEnabled")) scene.GetLayers().SetPhysicsEnabled(lid, ent["physicsEnabled"].get<bool>());
                    }
                }

                world.DestroyAll();
                scene.GetLayers().ClearEntityMembership();

                int loadedCount = 0;
                std::vector<ECS::Entity> restoredEntities;
                
                if (outEntityOrder) {
                    outEntityOrder->clear();
                }

                if (sceneJson.contains("Entities")) {
                    for (const auto& entityJson : sceneJson["Entities"]) {
                        ECS::Entity entity = Serialization::EntitySerializer::DeserializeEntity(world, entityJson);
                        ++loadedCount;
                        restoredEntities.push_back(entity);
                        
                        // Track entity order for hierarchy preservation
                        if (outEntityOrder) {
                            outEntityOrder->push_back(entity.Index);
                        }
                    }
                }

                // Restore hierarchy relationships if present
                if (sceneJson.contains("Hierarchy") && sceneJson["Hierarchy"].is_array()) {
                    const auto& hierarchyArray = sceneJson["Hierarchy"];
                    for (const auto& hierarchyEntry : hierarchyArray) {
                        if (!hierarchyEntry.contains("child") || !hierarchyEntry.contains("parent")) {
                            continue;
                        }
                        
                        size_t childIndex = hierarchyEntry["child"].get<size_t>();
                        size_t parentIndex = hierarchyEntry["parent"].get<size_t>();
                        
                        if (childIndex < restoredEntities.size() && parentIndex < restoredEntities.size()) {
                            ECS::Entity child = restoredEntities[childIndex];
                            ECS::Entity parent = restoredEntities[parentIndex];
                            world.Attach(child, parent);
                        }
                    }
                }

                LOG_DEBUG("Scene successfully loaded: "
                    << sceneJson.value("SceneName", "Unknown") << '\n'
                    << "\tVersion: " << sceneJson.value("Version", "Unknown") << '\n'
					<< "\tEntities loaded: " << loadedCount);

                scene.SetPath(filename);
                if (sceneJson.contains("SceneName")) {
                    scene.SetName(sceneJson["SceneName"].get<std::string>());
                }

                // Rebuild LayerManager membership lists so editor UI (LayersPanel)
                // sees the correct entities per layer. Deserialization writes the
                // Layer component directly to the world, which does not update the
                // Scene::LayerManager. Walk the restored entities and register
                // them with the LayerManager.
                for (const auto& ent : restoredEntities) {
                    if (!ent.IsNull() && world.IsAlive(ent) && world.Has<ECS::Components::Layer>(ent)) {
                        uint16_t lid = world.Get<ECS::Components::Layer>(ent).Id;
                        scene.GetLayers().OnLayerSet(ent, lid);
                    }
                }

                // Reconstruct prefab metadata for all instances
                // Converts legacy PrefabLink to runtime PrefabInstanceMetadata
                _reconstructPrefabMetadata(world, restoredEntities);

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

        /**
         * @brief Reloads the specified scene from disk, keeping the same scene slot.
         * @param index The index of the scene to restart.
         * @param filename Path to the scene file.
         * @return true if reload was successful, false otherwise.
         */
        bool RestartScene(const size_t index, const std::string& filename) {
            return LoadScene(index, filename);
        }

        /**
         * @brief Reloads the specified scene from disk, keeping the same scene slot.
         * @param index The index of the scene to restart.
         * @param filename Path to the scene file.
         * @param outEntityOrder Optional output vector to receive the loaded entity order.
         * @return true if reload was successful, false otherwise.
         */
        bool RestartScene(const size_t index, const std::string& filename, std::vector<uint32_t>* outEntityOrder) {
            return LoadScene(index, filename, outEntityOrder);
        }

    private:
        void _processPending() {
            if (m_pendingActive == NPOS)
                return;

            // Handle fade-aware transitions
            if (m_transitionMode != SceneTransitionMode::Immediate && m_audioSystem) {
                if (m_transitionMode == SceneTransitionMode::CrossFade) {
                    if (!m_crossfadeTriggered) {
                        m_audioSystem->OnSceneWillUnload(m_transitionFadeDuration, true);
                        m_crossfadeTriggered = true;
                    }
                }
                else if (!m_waitingForFadeOut) {
                    // Start fade-out on first call
                    m_audioSystem->OnSceneWillUnload(m_transitionFadeDuration, false);
                    m_waitingForFadeOut = true;
                    m_transitionFadeTimer = 0.0f;
                    const float maxRemaining = m_audioSystem->GetMaxFadeOutRemaining();
                    if (maxRemaining > m_transitionFadeDuration) {
                        m_transitionFadeDuration = maxRemaining;
                    }
                    return; // Don't transition yet, wait for fade
                }
                else {
                    // Update fade timer
                    m_transitionFadeTimer += static_cast<float>(TimeSystem::Instance().GetDeltaTime());

                    // Check if fade is complete
                    bool fadeComplete = !m_audioSystem->HasActiveFadeOuts();
                    bool timeoutReached = m_transitionFadeTimer >= (m_transitionFadeDuration + 0.1f); // Small buffer

                    if (!fadeComplete && !timeoutReached) {
                        return; // Still fading, wait
                    }

                    // Fade complete or timeout - proceed with transition
                    m_waitingForFadeOut = false;
                }
            }

            // Perform the actual transition
            _performTransition(m_pendingActive);
            m_pendingActive = NPOS;

            // Reset transition settings
            m_transitionMode = SceneTransitionMode::Immediate;
            m_transitionFadeDuration = 1.0f;
            m_crossfadeTriggered = false;
        }

        void _performTransition(const size_t toIndex) {
            if (toIndex >= m_scenes.size() || m_active == toIndex)
                return;

            // Notify listeners of scene change
            std::string oldName;
            const Scene* oldScene = nullptr;
            if (m_active != NPOS && m_active < m_scenes.size() && m_scenes[m_active]) {
                oldScene = m_scenes[m_active].get();
                oldName = m_scenes[m_active]->GetName();
            }

            // Get new scene name
            std::string newName;
            if (m_scenes[toIndex]) {
                newName = m_scenes[toIndex]->GetName();
            }

            m_active = toIndex;

            // Send scene changed message
            Messaging::MessageSystem::Notify(Messaging::SceneChanged{oldName, newName});

            // Unload resources owned by old scene
            const std::string oldTag = _makeOwnerTag(oldScene);
            if (!oldTag.empty()) {
                RM.UnloadAssetsByOwner(oldTag);
            }
        }

        // Generate a unique owner tag for a scene based on its pointer
        static std::string _makeOwnerTag(const Scene* scene) {
            if (!scene) {
                return {};
            }

            // Create a unique tag using the scene's memory address
            // Format: "Scene@<hex address>"
            // This is to ensure uniqueness per scene instance
            std::ostringstream oss;
            oss << "Scene@" << std::hex << reinterpret_cast<uintptr_t>(scene);
            return oss.str();
        }

        static std::string _resolveScenePath(const std::string& path) {
            if (path.empty()) {
                return {};
            }

            std::filesystem::path pathFs(path);
            return pathFs.is_absolute()
                ? pathFs.string()
                : Engine::ProjectPaths::ToAbsolutePath(path);
        }

        static std::string _normalizePath(const std::string& path) {
            std::filesystem::path normalized = std::filesystem::path(path).lexically_normal();
            std::string normalizedStr = normalized.string();
            std::replace(normalizedStr.begin(), normalizedStr.end(), '\\', '/');
            return normalizedStr;
        }

        size_t _findSceneIndexByPath(const std::string& normalizedAbsolutePath) const {
            const size_t count = m_scenes.size();
            for (size_t i = 0; i < count; ++i) {
                const Scene* scene = m_scenes[i].get();
                if (!scene) {
                    continue;
                }

                const std::string& scenePath = scene->GetPath();
                if (scenePath.empty()) {
                    continue;
                }

                const std::string sceneAbsolute = _resolveScenePath(scenePath);
                if (_normalizePath(sceneAbsolute) == normalizedAbsolutePath) {
                    return i;
                }
            }

            return NPOS;
        }

        // Initialize runtime prefab metadata post-load
        void _reconstructPrefabMetadata(ECS::World& world, const std::vector<ECS::Entity>& restoredEntities) {
            // Initialize PrefabManager if not already done
            if (!m_prefabManager) {
                m_prefabManager = std::make_unique<ECS::PrefabManager>(&world);
            }
            else {
                m_prefabManager->SetWorld(&world);
            }

            // Migrate legacy PrefabLink components to new PrefabInstanceMetadata format
            for (const auto& entity : restoredEntities) {
                if (!entity.IsNull() && world.IsAlive(entity)) {
                    // Check for legacy PrefabLink component and migrate it
                    if (world.Has<ECS::Components::PrefabLink>(entity)) {
                        const auto& link = world.Get<ECS::Components::PrefabLink>(entity);
                        std::string path = link.getPath();

                        if (!path.empty()) {
                            // Register prefab and get hash
                            uint32_t hash = m_prefabManager->RegisterPrefab(path);

                            // Create PrefabInstanceMetadata
                            ECS::Components::PrefabInstanceMetadata meta;
                            meta.PrefabHash = hash;
                            meta.Flags = 0;

                            // Add metadata component (replaces PrefabLink)
                            world.Add<ECS::Components::PrefabInstanceMetadata>(entity, meta);

                            // Remove old PrefabLink
                            world.Remove<ECS::Components::PrefabLink>(entity);

                            // Track instance in manager
                            m_prefabManager->TrackInstance(entity, hash);
                        }
                    }
                    // Track any existing PrefabInstanceMetadata
                    else if (world.Has<ECS::Components::PrefabInstanceMetadata>(entity)) {
                        const auto& meta = world.Get<ECS::Components::PrefabInstanceMetadata>(entity);
                        // Track instance in manager
                        m_prefabManager->TrackInstance(entity, meta.PrefabHash);
                    }
                }
            }
        }

        std::vector<std::unique_ptr<Scene>> m_scenes;
        std::unique_ptr<ECS::PrefabManager> m_prefabManager;
        static constexpr size_t NPOS = static_cast<size_t>(-1);
        size_t m_active = NPOS;
        size_t m_pendingActive = NPOS;

        // Audio fade transition support
        ECS::AudioSystem* m_audioSystem = nullptr;
        SceneTransitionMode m_transitionMode = SceneTransitionMode::Immediate;
        float m_transitionFadeDuration = 1.0f;
        float m_transitionFadeTimer = 0.0f;
        bool m_waitingForFadeOut = false;
        bool m_crossfadeTriggered = false;
    };
}

#endif
