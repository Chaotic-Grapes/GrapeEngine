/* Start Header *****************************************************************/
/*!
\file    Scene.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the Scene class.

Scene is a pure data container for a game scene, designed to be compatible with
a level editor. It stores:
- ECS World (entities and components)
- Layer management
- System profile (which systems to run)
- Scene metadata (name, path)

Scenes do NOT contain logic or virtual functions. All systems are executed by
the SceneManager using the SystemRegistry and SystemProfile.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef SCENE_H
#define SCENE_H

#include "ecs/Hierarchy.h"
#include "ecs/World.h"
#include "scene/LayerManager.h"
#include <memory>
#include <string>

namespace Scenes { class SceneManager; }
namespace Scenes {
    /**
     * @brief Pure data container for a game scene.
     * 
     * Scene stores all data for a scene without any logic or virtual functions.
     * This design allows scenes to be:
     * - Created and edited in a level editor
     * - Serialized to/from files (JSON, binary, etc.)
     * - Loaded without code compilation
     * 
     * The SceneManager is responsible for executing systems on the scene based
     * on its SystemProfile.
     */
    class Scene {
    public:
        Scene() = default;
        ~Scene() = default;

        // ********************** Scene Metadata ********************** //

        /**
         * @brief Gets the scene name.
         * @return The scene name.
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief Sets the scene name.
         * @param name The new scene name.
         */
        void SetName(const std::string& name) { m_name = name; }

        /**
         * @brief Gets the scene file path.
         * @return The scene file path.
         */
        const std::string& GetPath() const { return m_path; }

        /**
         * @brief Sets the scene file path.
         * @param path The new scene file path.
         */
        void SetPath(const std::string& path) { m_path = path; }

        // ********************** Core API ********************** //
        
        /**
         * @brief Access the ECS world for this scene.
         * @return Reference to the ECS::World instance.
         */
        ECS::World& GetWorld()                { return m_world; }

        /**
         * @brief Access the ECS world for this scene (const version).
         * @return Const reference to the ECS::World instance.
         */
        const ECS::World& GetWorld() const    { return m_world; }

        /**
         * @brief Access the LayerManager for this scene.
         * @return Reference to the LayerManager instance.
         */
        LayerManager& GetLayers()             { return m_layers; }

        /**
         * @brief Access the LayerManager for this scene (const version).
         * @return Const reference to the LayerManager instance.
         */
        const LayerManager& GetLayers() const { return m_layers; }

        // ********************** Entity Management ********************** //

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
         * @brief Creates a new entity on the specified layer with optional components.
         * @param layerId The ID of the layer to assign the entity to.
         * @param cs Variadic list of components to add to the entity.
         * @return The created ECS::Entity.
         */
        template<typename... TCs>
        ECS::Entity CreateOnLayer(const uint16_t layerId, TCs&&... cs) {
            const ECS::Entity e = m_world.Create(std::forward<TCs>(cs)...);
            m_world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{layerId});
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

            m_world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{id});
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

    private:
        friend class SceneManager;

        std::string m_name = "Untitled Scene";      ///< Scene name
        std::string m_path;                         ///< Scene file path
        ECS::World m_world;                         ///< ECS world containing all entities
        LayerManager m_layers;                      ///< Layer management
    };
}

#endif
