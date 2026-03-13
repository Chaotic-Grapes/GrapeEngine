/* Start Header *****************************************************************/
/*!
\file   EditorEntityActions.cpp
\author Samantha Leong (80%)
        Foo Rui Qin (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026

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
    // Marks the scene dirty only when the file menu pointer is valid
    // This helper centralizes the null check so call sites stay compact
    void MarkSceneDirtyIfNeeded(EditorFileMenu* fileMenu) {
        if (fileMenu) {
            fileMenu->MarkSceneDirty();
        }
    }
}

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

// Stores the initial scene pointer so action handlers can operate immediately after construction
EntityActions::EntityActions(Scenes::Scene* scene) : m_scene(scene) {}

// Replaces the active scene pointer when editor context changes
void EntityActions::SetScene(Scenes::Scene* scene) {
    m_scene = scene;
}

// -----------------------------------------------------------------------------
// Entity Creation and Destruction
// -----------------------------------------------------------------------------

// Creates a new entity with mandatory baseline components and optional parent attachment
EntityId EntityActions::AddEntity(const std::string& name, EntityId parent) {

    // Without an active scene there is no world to mutate so return invalid ID immediately
    if (!m_scene) return ECS::Entity::NPOS32;
    ECS::World& world = m_scene->GetWorld();

    // Create entity on default layer so LayerManager stays in sync
    ECS::Entity e = m_scene->CreateEntityOnLayer(0);

    // Store display name through string table helper so all name storage remains interned
    Editor::ECSUtils::SetEntityName(world, e, name);

    // LocalTransform is required because editor hierarchy and gizmos assume transform data always exists
    Editor::ECSUtils::SetComponent(&world, e, "LocalTransform", ECS::Components::LocalTransform{});

    // Active is required so newly created entities participate in activation flow consistently
    Editor::ECSUtils::SetComponent(&world, e, "Active", ECS::Components::Active{});

    // Ensure WorldTransform exists so hierarchy/system queries that require it see the entity
    // Initialize as dirty so systems will compute it on the next update
    ECS::Components::WorldTransform wt{};
    wt.Dirty = true;
    Editor::ECSUtils::SetComponent(&world, e, "WorldTransform", wt);

    // Resolve Layer component ID from hash and validate entity creation path assigned it correctly
    const uint32_t layerHash = Editor::ECSUtils::FNV1aHash("Layer");
    const ECS::ComponentTypeId layerIdFromHash = ECS::ComponentRegistry::GetComponentIdFromHash(layerHash);

    // Log error if Layer component is missing (should not happen)
    if (layerIdFromHash != ECS::NULL_COMPONENT_ID && !world.HasById(e, layerIdFromHash)) {
        LOG_ERROR("[EditorEntityActions] New entity missing Layer component (id=" << e.Index << ")");
    }

    // Parent attachment is optional and only performed when caller passed a concrete parent ID
    if (parent != ECS::Entity::NPOS32) {
        ECS::Entity p = world.Resolve(parent);
        if (world.IsAlive(p)) {
            world.Attach(e, p);
        }
    }

    // Record creation after successful setup so undo recreates the same initialized entity state
    if (m_undoSystem) { m_undoSystem->RecordEntityCreation(e.Index); }

    // Mark scene dirty so save prompts include this creation
    MarkSceneDirtyIfNeeded(m_fileMenu);
    return e.Index;
}

// Deletes an entity subtree and records one undo snapshot before destructive changes begin
bool EntityActions::RemoveEntity(EntityId id) {

    // Scene guard protects editor calls made during scene transitions or teardown
    if (!m_scene) return false;
    ECS::World& world = m_scene->GetWorld();

    // Resolve user-facing ID to runtime entity handle and stop when handle is stale
    ECS::Entity entity = world.Resolve(id);
    if (entity.IsNull() || !world.IsAlive(entity)) return false;

    // Record for undo system BEFORE deletion so the entity still exists for snapshotting
    if (m_undoSystem) { m_undoSystem->RecordEntityDeletion(id); }

    // Recursive delete ensures children are destroyed before parent so hierarchy traversal stays valid
    std::function<void(EntityId)> deleteRecursive = [&](EntityId entityId) {

        // Collect children first to avoid iterator invalidation
        std::vector<EntityId> children;
        ECS::Entity parent = world.Resolve(entityId);
        if (!parent.IsNull() && world.IsAlive(parent)) {
            world.ForChildren(parent, [&](ECS::Entity child) {
                children.push_back(child.Index);
            });
        }

        // Depth-first deletion guarantees no orphaned child references remain when parent is destroyed
        for (auto childId : children) {
            deleteRecursive(childId);
        }

        // Destroy current node after its descendants have already been removed
        ECS::Entity e = world.Resolve(entityId);
        if (!e.IsNull() && world.IsAlive(e)) {
            world.Destroy(e);
        }
    };

    deleteRecursive(id);

    // Mark scene dirty once after full subtree deletion completes
    MarkSceneDirtyIfNeeded(m_fileMenu);
    return true;
}



// Clears all entities except editor-only camera helpers that must survive scene reset operations
void EntityActions::ClearAllEntities() {

    // Scene guard prevents accidental world access during editor lifecycle transitions
    if (!m_scene) return;
    ECS::World& world = m_scene->GetWorld();

    // Gather first then destroy so iterator traversal is not invalidated mid-loop
    std::vector<ECS::Entity> allEntities;
    world.Each([&](ECS::Entity e) {
        // Keep editor camera
        if (Editor::ECSUtils::HasComponent(&world, e, "CameraEditor3D")) {
            return;
        }
        allEntities.push_back(e);
    });

    // Destroy in a second pass using stable copy collected above
    for (const auto& e : allEntities) {
        world.Destroy(e);
    }

    // Mark scene dirty because world content changed in bulk
    MarkSceneDirtyIfNeeded(m_fileMenu);
}

// -----------------------------------------------------------------------------
// Entity Hierarchy Operations
// -----------------------------------------------------------------------------

// Clones an entity subtree while preserving hierarchy structure and recording undo for the new root
EntityId EntityActions::CloneEntity(EntityId id) {

    // Scene guard prevents clone requests from running without a valid world context
    if (!m_scene) return ECS::Entity::NPOS32;
    ECS::World& world = m_scene->GetWorld();

    // Resolve original entity and reject stale IDs before any clone work
    ECS::Entity entity = world.Resolve(id);
    if (entity.IsNull() || !world.IsAlive(entity)) return ECS::Entity::NPOS32;

    // Tracks original-to-clone mapping which is useful for hierarchy integrity and future extension points
    std::unordered_map<EntityId, ECS::Entity> cloneMap;

    // Recursive clone duplicates parent first then recurses into children to rebuild subtree shape
    std::function<ECS::Entity(EntityId, EntityId)> cloneRecursive =
        [&](EntityId entityId, EntityId newParentId) -> ECS::Entity {

        ECS::Entity original = world.Resolve(entityId);
        if (original.IsNull() || !world.IsAlive(original)) {
            return ECS::NULL_ENTITY;
        }

        // World clone performs component-level duplication in one call
        ECS::Entity clone = world.Clone(original);

        // Append clone suffix so user can distinguish original from duplicate in hierarchy
        if (auto* name = Editor::ECSUtils::GetNamePtrMutable(world, clone)) {
            std::string baseName = ECS::StringTable::Resolve(name->Value);
            if (baseName.empty()) {
                baseName = "Entity";
            }
            std::string newName = baseName + " (Clone)";
            name->Value = ECS::StringTable::Intern(newName);
        }

        // Parenting branch preserves relative hierarchy when cloning into an existing parent
        if (newParentId != ECS::Entity::NPOS32) {
            ECS::Entity newParent = world.Resolve(newParentId);
            if (!newParent.IsNull() && world.IsAlive(newParent)) {
                world.Attach(clone, newParent);
            }
        }
        else {
            // Remove parent component if cloning as root
            if (world.Has<ECS::Components::Parent>(clone)) {
                world.Detach(clone);
            }
        }

        // Store mapping
        cloneMap[entityId] = clone;

        // Collect children from original so recursive calls mirror original subtree ordering
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

    // Clone root with original parent so duplicate appears at same hierarchy depth by default
    ECS::Entity cloned = cloneRecursive(id, originalParentId);
    if (!cloned.IsNull() && world.IsAlive(cloned)) {
        if (m_undoSystem) {
            // Record clone creation so undo removes the full cloned hierarchy
            m_undoSystem->RecordEntityCreation(cloned.Index);
        }
    }

    // Mark scene dirty so clone operation is treated as unsaved content change
    MarkSceneDirtyIfNeeded(m_fileMenu);
    return cloned.Index;
}

// Reparents one entity while preventing cycles and recording undo only on real hierarchy changes
void EntityActions::ReparentEntity(EntityId child, EntityId newParent) {

    // Scene guard avoids running hierarchy logic when no active world exists
    if (!m_scene) return;
    ECS::World& world = m_scene->GetWorld();

    // Resolve the child entity and bail if it no longer exists
    ECS::Entity childEntity = world.Resolve(child);
    if (childEntity.IsNull() || !world.IsAlive(childEntity)) return;

    // Save the current parent before any changes so undo knows where to restore to
    const ECS::Entity currentParentEntity = world.ParentOf(childEntity);
    const EntityId oldParent = currentParentEntity.IsNull() ? ECS::Entity::NPOS32 : currentParentEntity.Index;

    // Tracks whether we attempted a structural change that should be validated for undo and dirty-state
    bool changed = false;

    if (newParent != ECS::Entity::NPOS32) {
        // Resolve the target parent and bail if it no longer exists
        ECS::Entity newParentEntity = world.Resolve(newParent);
        if (newParentEntity.IsNull() || !world.IsAlive(newParentEntity)) return;

        // Walk up ancestry to prevent attaching child under its own descendant
        // If it does, attaching would create a cycle (e.g. A -> B -> A)
        bool isDescendant = false;
        ECS::Entity checkParent = newParentEntity;
        while (!checkParent.IsNull()) {
            if (checkParent.Index == child) {
                isDescendant = true;
                break;
            }
            checkParent = world.ParentOf(checkParent);
        }

        if (!isDescendant) {
            // Safe to attach: attach handles hierarchy index updates internally
            world.Attach(childEntity, newParentEntity);
            changed = true;
        }
        else {
            LOG_WARNING("Cannot parent entity to its own descendant: this would create a cyclic hierarchy");
        }
    }
    else {
        // newParent is NPOS32, so promote child to a root entity with no parent
        world.Detach(childEntity);
        changed = true;
    }

    // Re-resolve handles because attach and detach can move packed entity storage internally
    const ECS::Entity updatedChild = world.Resolve(child);
    const ECS::Entity updatedParentEntity = (!updatedChild.IsNull() && world.IsAlive(updatedChild))
        ? world.ParentOf(updatedChild)
        : ECS::NULL_ENTITY;

    // Read effective parent after operation because runtime may reject attach in edge conditions
    const EntityId actualParent = updatedParentEntity.IsNull() ? ECS::Entity::NPOS32 : updatedParentEntity.Index;

    // Only push an undo entry if the hierarchy actually changed
    if (changed && actualParent != oldParent && m_undoSystem) {
        m_undoSystem->RecordEntityReparent(child, oldParent, actualParent);
    }

    // Mark scene dirty only when parent relationship actually differs from original state
    if (changed && actualParent != oldParent) {
        MarkSceneDirtyIfNeeded(m_fileMenu);
    }
}
