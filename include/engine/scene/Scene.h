/* Start Header *****************************************************************/
/*!
\file    Scene.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the Scene
class, responsible for managing a single scene in the application. It provides
methods for updating the scene, handling events, and managing entities within the
scene.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef SCENE_H
#define SCENE_H

#include "core/Profiler.h"
#include "ecs/Hierarchy.h"
#include "ecs/World.h"
#include "scene/LayerManager.h"
#include <functional>
#include <memory>
#include <vector>

namespace Scenes { class SceneManager; }
namespace Scenes {
    class Scene {
    public:
        using System = std::function<void(Scene&, float)>;
        struct SystemEntry {
        public:
            // Stable diagnostic identity (monotonic, never reused)
            uint64_t Id = 0;
            // Optional debug label; not owned, expected to point to a static string or externally managed lifetime
            const char* Name = nullptr;
            // Runtime toggle (not needed for read-only introspection, but useful for scheduling control)
            bool Enabled = true;
            // The actual system callback
            System Callback;
        };

    public:
        virtual ~Scene() = default;

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
         * @brief Access the ECS world for this scene.
         * @return Reference to the ECS::World instance.
         */
        ECS::World& GetWorld() { return m_world; }

        /**
         * @brief Access the ECS world for this scene (const version).
         * @return Const reference to the ECS::World instance.
         */
        const ECS::World& GetWorld() const { return m_world; }

        /**
         * @brief Access the LayerManager for this scene.
         * @return Reference to the LayerManager instance.
         */
        LayerManager& GetLayers() { return m_layers; }

        /**
         * @brief Access the LayerManager for this scene (const version).
         * @return Const reference to the LayerManager instance.
         */
        const LayerManager& GetLayers() const { return m_layers; }

        /**
         * @brief Creates an empty entity in the scene with an optional parent.
         * @param parent Optional parent entity to establish hierarchy.
         * @return The created ECS::Entity.
         * @note Use m_world.Create(arguments...) to create entities with components.
         */
        ECS::Entity CreateEntity(const std::optional<ECS::Entity> parent = std::nullopt) {
            // Will this suppress copy elision?
            // Consideration: Let user attach after creation to avoid this?
            const ECS::Entity e = m_world.Create();
            if (parent.has_value())
                m_world.Attach(e, parent.value());

            return e;
        }

        /**
         * @brief Destroys an entity in the scene.
         * @param entity The entity to destroy.
         */
        void DestroyEntity(const ECS::Entity entity) { m_world.Destroy(entity); }

        /**
         * @brief Adds system(s) to the scene's update loop.
         * @param sys The system function to add.
         * @param name Optional name for the system (for debugging/diagnostics).
         * @return The stable Id of the added system.
         * @note To use this function, use a lambda or std::function matching the System signature.
         * Example:
         * ```cpp
         * AddSystem([](Scenes::Scene& s, float dt) {
         *     ECS::LifetimeSystem::Update(s.GetWorld(), dt);
         *     ECS::CameraSystem::Update(s.GetWorld());
         * }, "Multiple Systems");
         * ```
         * Alternatively, add single systems:
         * ```cpp
         * AddSystem([](Scenes::Scene& s, float dt) {
         *     ECS::LifetimeSystem::Update(s.GetWorld(), dt);
         * }, "Lifetime System");
         * ```
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

        /**
         * @brief Creates a new entity on the specified layer with optional components.
         * @param layerId The ID of the layer to assign the entity to.
         * @param cs Variadic list of components to add to the entity.
         * @return The created ECS::Entity.
         */
        template<typename... TCs>
        ECS::Entity CreateOnLayer(const uint16_t layerId, TCs&&... cs) {
            const ECS::Entity e = m_world.Create(std::forward<TCs>(cs)...);
            m_world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{ layerId });
            m_layers.OnLayerSet(e, layerId);

            return e;
        }

        /**
         * @brief Sets the layer of an entity.
         * @param e The entity to modify.
         * @param id The ID of the layer to assign to the entity.
         */
        void SetLayer(const ECS::Entity e, const uint16_t id) {
            if (m_world.Has<ECS::Components::Layer>(e)) {
                const auto prev = m_world.Get<ECS::Components::Layer>(e).Id;
                if (prev == id)
                    return;

                m_layers.OnLayerRemoved(e, prev);
            }

            m_world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{ id });
            m_layers.OnLayerSet(e, id);
        }

        /**
         * @brief Removes the layer component from an entity.
         * @param entity The entity to modify.
         */
        void RemoveFromLayer(const ECS::Entity entity) {
            if (!m_world.Has<ECS::Components::Layer>(entity))
                return;

            const auto prev = m_world.Get<ECS::Components::Layer>(entity).Id;
            m_layers.OnLayerRemoved(entity, prev);
            m_world.Remove<ECS::Components::Layer>(entity);
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

        /**
         * @brief Iterates over each system entry, invoking the provided function.
         * @tparam TFn The type of the function to invoke for each system.
         */
        template<typename TFn>
        void ForEachSystem(TFn&& fn) const {
            for (size_t i = 0; i < m_systems.size(); ++i) {
                fn(m_systems[i].Id, i, m_systems[i]);
            }
        }

        /**
         * @brief Finds the index of a system by its name.
         * @param name The name of the system to find.
         */
        size_t FindSystemIndexByName(const char* name) const {
            if (!name)
                return static_cast<size_t>(-1);

            for (size_t i = 0; i < m_systems.size(); ++i) {
                if (m_systems[i].Name && std::strcmp(m_systems[i].Name, name) == 0) {
                    return i;
                }
            }

            return static_cast<size_t>(-1);
        }

        /**
         * @brief Finds the stable Id of a system by its name.
         * @param name The name of the system to find.
         */
        uint64_t FindSystemIdByName(const char* name) const {
            if (!name)
                return 0ull;

            for (auto const& s : m_systems) {
                if (s.Name && std::strcmp(s.Name, name) == 0) {
                    return s.Id;
                }
            }

            return 0ull;
        }

        /**
         * @brief Finds the index of a system by its stable Id.
         * @param id The stable Id of the system to find.
         */
        size_t FindSystemIndexById(const uint64_t id) const {
            for (size_t i = 0; i < m_systems.size(); ++i) {
                if (m_systems[i].Id == id)
                    return i;
            }

            return static_cast<size_t>(-1);
        }

    private:
        // Core update: runs enabled systems (in order), then updates transform hierarchy        
        void _update(const float dt) {
            // ECS::World::DeferGuard guard(m_world);
            for (auto& s : m_systems) {
                if (s.Enabled && s.Callback) {
                    Profiler::Get().BeginScope(s.Name ? s.Name : "Unknown System");
                    s.Callback(*this, dt);
                    Profiler::Get().EndScope(s.Name ? s.Name : "Unknown System");
                }
            }
            ECS::Hierarchy::UpdateTransforms(m_world);
        }

        friend class SceneManager;
        ECS::World m_world;
        LayerManager m_layers;
        std::vector<SystemEntry> m_systems;
        uint64_t m_nextSystemId = 1; // 0 reserved as "invalid"
    };
}

#endif