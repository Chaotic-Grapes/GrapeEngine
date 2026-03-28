/* Start Header *****************************************************************/
/*!
\file    Scene.h
\author  Muhammad Nur Fadzly Bin Zulkifli (95%)
         Foo Rui Qin (5%)
\date    12th March 2026
\par     muhammadnurfadzly.b@digipen.edu
         ruiqin.foo@digipen.edu
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

#include "ecs/World.h"
#include "scene/LayerManager.h"
#include "Export.h"
#include <memory>
#include <string>

namespace Scenes { class SceneManager; }
namespace Scenes {
    // Pure data container for a game scene
    // Stores scene data only and leaves execution logic to SceneManager
    class GRAPEENGINE_API Scene {
    public:
        /**
         * @brief Construct a scene and bind its layer manager to the world.
         */
        Scene() {
            // Set LayerManager pointer on World so systems can access it
            m_world.SetLayerManager(&m_layers);
        }

        // Use default destruction because all members are value-owned
        ~Scene() = default;

        // ********************** Scene Metadata ********************** //

        // Return the scene display name used by editor and serialization
        const std::string& GetName() const { return m_name; }

        // Set the scene display name
        void SetName(const std::string& name) { m_name = name; }

        // Return the file path associated with this scene
        const std::string& GetPath() const { return m_path; }

        // Set the file path associated with this scene
        void SetPath(const std::string& path) { m_path = path; }

        // ********************** Core API ********************** //

        // Return mutable ECS world access for scene editing and system updates
        ECS::World& GetWorld()                { return m_world; }

        // Return read-only ECS world access
        const ECS::World& GetWorld() const    { return m_world; }

        // Return mutable layer manager access
        LayerManager& GetLayers()             { return m_layers; }

        // Return read-only layer manager access
        const LayerManager& GetLayers() const { return m_layers; }

        // ********************** Entity Management ********************** //

        // Create an empty entity, mark it active by default, and optionally attach it to a parent
        ECS::Entity CreateEntity(const std::optional<ECS::Entity> parent = std::nullopt) {
            // Will this suppress copy elision?
            // Consideration: Let user attach after creation to avoid this?
            const ECS::Entity e = m_world.Create();

            // Set Active by default so new entities are enabled
            m_world.Set<ECS::Components::Active>(e, ECS::Components::Active{ true });
            if (parent.has_value())
                m_world.Attach(e, parent.value());

            return e;
        }

        /**
         * @brief Create an empty entity on a layer and optionally attach a parent.
         * @param layerId Target layer id.
         * @param parent Optional parent entity.
         * @return Newly created entity.
         */
        ECS::Entity CreateEntityOnLayer(const uint16_t layerId, const std::optional<ECS::Entity> parent = std::nullopt);

        // Destroy an entity and all ECS state tied to it
        void DestroyEntity(const ECS::Entity entity) { m_world.Destroy(entity); }

        // Create an entity with provided components, ensure Active exists, then assign layer membership
        template<typename... TCs>
        ECS::Entity CreateOnLayer(const uint16_t layerId, TCs&&... cs) {
            const ECS::Entity e = m_world.Create(std::forward<TCs>(cs)...);

            // Set Active by default so new entities are enabled
            if (!m_world.Has<ECS::Components::Active>(e)) {
                m_world.Set<ECS::Components::Active>(e, ECS::Components::Active{ true });
            }
            m_world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{layerId});
            m_layers.OnLayerSet(e, layerId);

            return e;
        }

        // Move an entity to a new layer and synchronize collider layer masks
        void SetLayer(const ECS::Entity e, const uint16_t id) {
            if (m_world.Has<ECS::Components::Layer>(e)) {
                const auto prev = m_world.Get<ECS::Components::Layer>(e).Id;
                if (prev == id)
                    return;

                m_layers.OnLayerRemoved(e, prev);
            }

            m_world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{id});
            m_layers.OnLayerSet(e, id);

            // Synchronize the collider's LayerMask with the layer manager's collision mask
            // This ensures physics collision checks use the correct layer-based collision rules
            _syncCollidersToLayer(e, id);
        }

        /**
         * @brief Set an entity layer using component registry hash lookup.
         * @param e Entity to move.
         * @param id Target layer id.
         */
        void SetLayerById(const ECS::Entity e, const uint16_t id);

        // Update collider masks to match the layer collision mask configured in LayerManager
        void _syncCollidersToLayer(const ECS::Entity e, const uint16_t layerId) {
            uint32_t layerMask = m_layers.GetLayerMask(layerId);
            if (m_world.Has<ECS::Components::CircleCollider2D>(e)) {
                auto& circle = m_world.Get<ECS::Components::CircleCollider2D>(e);
                circle.LayerMask = layerMask;
            }
            if (m_world.Has<ECS::Components::BoxCollider2D>(e)) {
                auto& box = m_world.Get<ECS::Components::BoxCollider2D>(e);
                box.LayerMask = layerMask;
            }
        }

        // Remove layer membership from an entity and notify the layer manager
        void RemoveFromLayer(const ECS::Entity entity) {
            if (!m_world.Has<ECS::Components::Layer>(entity))
                return;

            const auto prev = m_world.Get<ECS::Components::Layer>(entity).Id;
            m_layers.OnLayerRemoved(entity, prev);
            m_world.Remove<ECS::Components::Layer>(entity);
        }

    private:
        friend class SceneManager;

        std::string m_name = "Untitled Scene";      // Scene name
        std::string m_path;                         // Scene file path
        ECS::World m_world;                         // ECS world containing all entities
        LayerManager m_layers;                      // Layer management
    };
}

#endif
