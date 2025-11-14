/* Start Header *****************************************************************/
/*!
\file    TestScene.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of the TestScene class, which provides
backward compatibility for test scenes that need the old Scene API with
virtual lifecycle hooks and AddSystem() functionality.

TestScene is for ENGINE TESTING ONLY. Production game scenes should use
the pure data Scene class for editor compatibility.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef TESTSCENE_H
#define TESTSCENE_H

#include "core/Profiler.h"
#include "ecs/Hierarchy.h"
#include "scene/Scene.h"
#include <functional>
#include <vector>

namespace Scenes {
    /**
     * @brief Legacy Scene implementation for test scenes only.
     * 
     * TestScene provides the old Scene API with virtual lifecycle hooks and
     * AddSystem() functionality. This allows test scenes to continue working
     * without modification while production scenes use the new data-driven
     * Scene class.
     * 
     * @warning This class is for ENGINE TESTING ONLY. Do not use for production
     *          game scenes. Use the pure data Scene class instead.
     */
    class TestScene {
    public:
        using System = std::function<void(Scene&, float)>;
        struct SystemEntry {
            uint64_t Id = 0;
            const char* Name = nullptr;
            bool Enabled = true;
            System Callback;
        };

        virtual ~TestScene() = default;

        // ********************** Lifecycle hooks ********************** //

        /**
         * @brief Called once when the scene is added to the SceneManager.
         * Use this method to perform any necessary setup, such as loading assets
         */
        virtual void OnLoad() {}

        /**
         * @brief Called once when the scene is removed from the SceneManager.
         * Use this method to perform any necessary cleanup, such as releasing resources.
         */
        virtual void OnUnload() {}

        /**
         * @brief Called whenever the scene becomes the active scene.
         * Use this method to initialize or reset any state needed when the scene is activated.
         */
        virtual void OnEnter() {}

        /**
         * @brief Called every frame to update the scene.
         */
        virtual void OnUpdate() {}

        /**
         * @brief Called at fixed time intervals to perform physics updates.
         * Use this method for physics simulations or other time-sensitive updates.
         */
        virtual void OnFixedUpdate() {}

        /**
         * @brief Called every frame after OnUpdate to perform late updates.
         * Use this method for operations that need to occur after all regular updates.
         */
        virtual void OnLateUpdate() {}

        /**
         * @brief Called whenever the scene stops being the active scene.
         * Use this method to pause or save any state needed when the scene is deactivated.
         */
        virtual void OnExit() {}

        // ********************** Core API ********************** //
        
        /**
         * @brief Access the underlying Scene data.
         * @return Reference to the Scene instance.
         */
        Scene& GetScene() { return m_scene; }

        /**
         * @brief Access the underlying Scene data (const version).
         * @return Const reference to the Scene instance.
         */
        const Scene& GetScene() const { return m_scene; }

        /**
         * @brief Access the ECS world for this scene.
         * @return Reference to the ECS::World instance.
         */
        ECS::World& GetWorld() { return m_scene.GetWorld(); }

        /**
         * @brief Access the ECS world for this scene (const version).
         * @return Const reference to the ECS::World instance.
         */
        const ECS::World& GetWorld() const { return m_scene.GetWorld(); }

        /**
         * @brief Access the LayerManager for this scene.
         * @return Reference to the LayerManager instance.
         */
        LayerManager& GetLayers() { return m_scene.GetLayers(); }

        /**
         * @brief Access the LayerManager for this scene (const version).
         * @return Const reference to the LayerManager instance.
         */
        const LayerManager& GetLayers() const { return m_scene.GetLayers(); }

        // ********************** Entity Management ********************** //

        /**
         * @brief Creates an empty entity in the scene with an optional parent.
         * @param parent Optional parent entity to establish hierarchy.
         * @return The created ECS::Entity.
         */
        ECS::Entity CreateEntity(const std::optional<ECS::Entity> parent = std::nullopt) {
            return m_scene.CreateEntity(parent);
        }

        /**
         * @brief Destroys an entity in the scene.
         * @param entity The entity to destroy.
         */
        void DestroyEntity(const ECS::Entity entity) {
            m_scene.DestroyEntity(entity);
        }

        /**
         * @brief Creates a new entity on the specified layer with optional components.
         * @param layerId The ID of the layer to assign the entity to.
         * @param cs Variadic list of components to add to the entity.
         * @return The created ECS::Entity.
         */
        template<typename... TCs>
        ECS::Entity CreateOnLayer(const uint16_t layerId, TCs&&... cs) {
            return m_scene.CreateOnLayer(layerId, std::forward<TCs>(cs)...);
        }

        /**
         * @brief Sets the layer of an entity.
         * @param e The entity to modify.
         * @param id The ID of the layer to assign to the entity.
         */
        void SetLayer(const ECS::Entity e, const uint16_t id) {
            m_scene.SetLayer(e, id);
        }

        /**
         * @brief Removes the layer component from an entity.
         * @param entity The entity to modify.
         */
        void RemoveFromLayer(const ECS::Entity entity) {
            m_scene.RemoveFromLayer(entity);
        }

        /**
         * @brief Adds system(s) to the scene's update loop.
         * @param sys The system function to add.
         * @param name Optional name for the system (for debugging/diagnostics).
         * @return The stable Id of the added system.
         */
        uint64_t AddSystem(System sys, const char* name = nullptr) {
            SystemEntry entry;
            entry.Id = m_nextSystemId++;
            entry.Name = name;
            entry.Enabled = true;
            entry.Callback = std::move(sys);
            m_systems.emplace_back(std::move(entry));
            
            return m_systems.back().Id;
        }

        // ********************** Read-Only API for Diagnostics ********************** //

        /**
         * @brief Gets the count of registered systems in the scene.
         * @return The number of systems.
         */
        size_t GetSystemCount() const { return m_systems.size(); }

        /**
         * @brief Gets a system entry by index.
         * @param index The index of the system to retrieve.
         */
        const SystemEntry* GetSystem(const size_t index) const {
            if (index >= m_systems.size())
                return nullptr;
            return &m_systems[index];
        }

        /**
         * @brief Gets a const reference to the vector of all system entries.
         * @return Const reference to the vector of SystemEntry.
         */
        const std::vector<SystemEntry>& GetSystems() const { return m_systems; }

    private:
        // Core update: runs enabled systems (in order), then updates transform hierarchy
        void _update(const float dt) {
            for (auto& s : m_systems) {
                if (s.Enabled && s.Callback) {
                    Profiler::Get().BeginScope(s.Name ? s.Name : "Unknown System");
                    s.Callback(m_scene, dt);
                    Profiler::Get().EndScope(s.Name ? s.Name : "Unknown System");
                }
            }
            ECS::Hierarchy::UpdateTransforms(m_scene.GetWorld());
        }

        friend class TestSceneManager;
        Scene m_scene;
        std::vector<SystemEntry> m_systems;
        uint64_t m_nextSystemId = 1; // 0 reserved as "invalid"
    };
}

#endif // TESTSCENE_H
