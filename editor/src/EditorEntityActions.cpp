/* Start Header *****************************************************************/
/*!
\file   EditorEntityActions.cpp
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   16th November 2025

\brief
Implements editor-side entity operations such as creation, deletion, cloning
and reparenting for the active scene. Called by panels so editor scripts avoid
direct ECS manipulation.
*/
/* End Header *******************************************************************/

#include "EditorEntityActions.h"
#include "serialization/EntitySerializer.h"
#include "ecs/World.h"
#include "ecs/Components.h" 
#include "ecs/ComponentRegistry.h"
#include "ecs/StringTable.h"
#include "EditorECSUtils.h"
#include "core/Logger.h"
#include "UndoSystem.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace {
    void MarkSceneDirtyIfNeeded(EditorFileMenu* fileMenu) {
        if (fileMenu) {
            fileMenu->MarkSceneDirty();
        }
    }
}

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

// Store initial scene reference so all editor actions work immediately
EntityActions::EntityActions(Scenes::Scene* scene) : m_scene(scene) {}

// Update the scene pointer when the editor loads or switches scenes
void EntityActions::SetScene(Scenes::Scene* scene) {
    m_scene = scene;
}

// -----------------------------------------------------------------------------
// Entity Creation and Destruction
// -----------------------------------------------------------------------------

// Add a new entity to the scene
EntityId EntityActions::AddEntity(const std::string& name, EntityId parent) {
    if (!m_scene) return ECS::Entity::NPOS32;
    ECS::World& world = m_scene->GetWorld();

    // Create entity on default layer so LayerManager stays in sync
    ECS::Entity e = m_scene->CreateEntityOnLayer(0);

    // Name (StringId)
    Editor::ECSUtils::SetEntityName(world, e, name);

    // Mandatory LocalTransform
    Editor::ECSUtils::SetComponent(&world, e, "LocalTransform", ECS::Components::LocalTransform{});

    // Ensure WorldTransform exists so hierarchy/system queries that require it see the entity
    // Initialize as dirty so systems will compute it on the next update
    ECS::Components::WorldTransform wt{};
    wt.Dirty = true;
    Editor::ECSUtils::SetComponent(&world, e, "WorldTransform", wt);

    // Verify Layer component exists
    const uint32_t layerHash = Editor::ECSUtils::FNV1aHash("Layer");
    const ECS::ComponentTypeId layerIdFromHash = ECS::ComponentRegistry::GetComponentIdFromHash(layerHash);

    // Log error if Layer component is missing (should not happen)
    if (layerIdFromHash != ECS::NULL_COMPONENT_ID && !world.HasById(e, layerIdFromHash)) {
        LOG_ERROR("[EditorEntityActions] New entity missing Layer component (id=" << e.Index << ")");
    }

    // Optional parent
    if (parent != ECS::Entity::NPOS32) {
        ECS::Entity p = world.Resolve(parent);
        if (world.IsAlive(p)) {
            world.Attach(e, p);
        }
    }

    // For undo system
    if (m_undoSystem) { m_undoSystem->RecordEntityCreation(e.Index); }

    // MARK SCENE AS DIRTY
    MarkSceneDirtyIfNeeded(m_fileMenu);
    return e.Index;
}

// Delete an entity and all its children from the scene
void EntityActions::RemoveEntity(EntityId id) {
    if (!m_scene) return;
    ECS::World& world = m_scene->GetWorld();

    ECS::Entity entity = world.Resolve(id);
    if (entity.IsNull() || !world.IsAlive(entity)) return;

    // Record for undo system BEFORE deletion so the entity still exists for snapshotting
    if (m_undoSystem) { m_undoSystem->RecordEntityDeletion(id); }

    // Recursive lambda to delete entity and all children
    std::function<void(EntityId)> deleteRecursive = [&](EntityId entityId) {
        // Collect children first to avoid iterator invalidation
        std::vector<EntityId> children;
        ECS::Entity parent = world.Resolve(entityId);
        if (!parent.IsNull() && world.IsAlive(parent)) {
            world.ForChildren(parent, [&](ECS::Entity child) {
                children.push_back(child.Index);
            });
        }

        // Recursively delete all children
        for (auto childId : children) {
            deleteRecursive(childId);
        }

        // Finally delete this entity
        ECS::Entity e = world.Resolve(entityId);
        if (!e.IsNull() && world.IsAlive(e)) {
            world.Destroy(e);
        }
        };

    deleteRecursive(id);

    // MARK SCENE AS DIRTY
    MarkSceneDirtyIfNeeded(m_fileMenu);
}



// Remove every entity in the scene
void EntityActions::ClearAllEntities() {
    if (!m_scene) return;
    ECS::World& world = m_scene->GetWorld();

    std::vector<ECS::Entity> allEntities;
    world.Each([&](ECS::Entity e) {
        // Keep editor camera
        if (Editor::ECSUtils::HasComponent(&world, e, "CameraEditor3D")) {
            return;
        }
        allEntities.push_back(e);
        });

    for (const auto& e : allEntities) {
        world.Destroy(e);
    }

    // MARK SCENE AS DIRTY
    MarkSceneDirtyIfNeeded(m_fileMenu);
}

// -----------------------------------------------------------------------------
// Entity Hierarchy Operations
// -----------------------------------------------------------------------------

// Clone an entity including all components and children
EntityId EntityActions::CloneEntity(EntityId id) {
    if (!m_scene) return ECS::Entity::NPOS32;
    ECS::World& world = m_scene->GetWorld();

    ECS::Entity entity = world.Resolve(id);
    if (entity.IsNull() || !world.IsAlive(entity)) return ECS::Entity::NPOS32;

    // Map to track original entity -> clone entity for maintaining hierarchy
    std::unordered_map<EntityId, ECS::Entity> cloneMap;

    // Recursive lambda to clone entity and all its children
    std::function<ECS::Entity(EntityId, EntityId)> cloneRecursive =
        [&](EntityId entityId, EntityId newParentId) -> ECS::Entity {

        ECS::Entity original = world.Resolve(entityId);
        if (original.IsNull() || !world.IsAlive(original)) {
            return ECS::NULL_ENTITY;
        }

        // Clone the entity (this copies all components)
        ECS::Entity clone = world.Clone(original);

        // Update name to indicate it's a clone
        if (auto* name = Editor::ECSUtils::GetNamePtrMutable(world, clone)) {
            std::string baseName = ECS::StringTable::Resolve(name->Value);
            if (baseName.empty()) {
                baseName = "Entity";
            }
            std::string newName = baseName + " (Clone)";
            name->Value = ECS::StringTable::Intern(newName);
        }

        // Set parent relationship
        if (newParentId != ECS::Entity::NPOS32) {
            ECS::Entity newParent = world.Resolve(newParentId);
            if (!newParent.IsNull() && world.IsAlive(newParent)) {
                world.Attach(clone, newParent);
            }
        } else {
            // Remove parent component if cloning as root
            if (world.Has<ECS::Components::Parent>(clone)) {
                world.Detach(clone);
            }
        }

        // Store mapping
        cloneMap[entityId] = clone;

        // Find and clone all children
        std::vector<EntityId> children;
        world.ForChildren(original, [&](ECS::Entity child) {
            children.push_back(child.Index);
        });

        // Recursively clone children with this clone as their parent
        for (auto childId : children) {
            cloneRecursive(childId, clone.Index);
        }

        return clone;
        };

    // Get the parent of the original entity (if any)
    EntityId originalParentId = ECS::Entity::NPOS32;
    const ECS::Entity originalParent = world.ParentOf(entity);
    if (!originalParent.IsNull()) {
        originalParentId = originalParent.Index;
    }

    // Clone the entity hierarchy
    ECS::Entity cloned = cloneRecursive(id, originalParentId);

    // MARK SCENE AS DIRTY
    MarkSceneDirtyIfNeeded(m_fileMenu);
    return cloned.Index;
}

// Move an entity under a new parent in the hierarchy
void EntityActions::ReparentEntity(EntityId child, EntityId newParent) {
    if (!m_scene) return;
    ECS::World& world = m_scene->GetWorld();

    ECS::Entity childEntity = world.Resolve(child);
    if (childEntity.IsNull() || !world.IsAlive(childEntity)) return;

    // Handle setting a new parent
    if (newParent != ECS::Entity::NPOS32) {
        ECS::Entity newParentEntity = world.Resolve(newParent);
        if (newParentEntity.IsNull() || !world.IsAlive(newParentEntity)) return;

        // Check for circular parenting (child cannot be ancestor of new parent)
        bool isDescendant = false;
        ECS::Entity checkParent = newParentEntity;
        while (!checkParent.IsNull()) {
            if (checkParent.Index == child) {
                // Circular dependency detected
                isDescendant = true;
                break;
            }
            checkParent = world.ParentOf(checkParent);
        }

        if (!isDescendant) {
            // Use Attach to set the new parent (handles hierarchy index updates automatically)
            world.Attach(childEntity, newParentEntity);
        }
        else {
            LOG_WARNING("Cannot parent entity to its own descendant: this would create a cyclic hierarchy");
        }
    }
    else {
        // Use Detach to remove parent (make root entity)
        world.Detach(childEntity);
    }

    // MARK SCENE AS DIRTY (only if operation succeeded)
    MarkSceneDirtyIfNeeded(m_fileMenu);
}
