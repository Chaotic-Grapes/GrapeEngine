/* Start Header *****************************************************************/
/*!
\file   UndoSystem.cpp
\author Samantha Leong Sher Yen (70%)
        Foo Rui Qin (30%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
Implementation of the UndoSystem and command types used to support
undo/redo operations within the editor. This system records user actions,
applies them via command objects, and restores previous states when
undoing or redoing actions.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "UndoSystem.h"
#include "services/Input.h"
#include "core/Logger.h"
#include "EditorECSUtils.h"
#include "serialization/EntitySerializer.h"
#include "../include/core/World/TileMap.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

using Json = nlohmann::json;

/**
 * @brief Compares two JSON values with tolerance for floating point numbers.
 *
 * Performs a recursive comparison between two JSON objects. Floating point
 * numbers are compared using an epsilon tolerance to avoid precision issues,
 * while arrays and objects are compared element-by-element.
 *
 * @param a First JSON value to compare.
 * @param b Second JSON value to compare.
 * @param epsilon Allowed floating-point difference tolerance.
 * @return True if the JSON values are approximately equal, otherwise false.
 */
static bool JsonApproxEqual(const nlohmann::json& a, const nlohmann::json& b, float epsilon = 1e-5f) {
    // If the types differ, they cannot be equal
    if (a.type() != b.type()) return false;
    // Handle floating point comparison with epsilon tolerance
    if (a.is_number_float()) {
        return std::abs(a.get<float>() - b.get<float>()) < epsilon;
    }
    // Recursively compare arrays
    if (a.is_array()) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) {
            if (!JsonApproxEqual(a[i], b[i], epsilon)) return false;
        }
        return true;
    }
    // Recursively compare objects
    if (a.is_object()) {
        if (a.size() != b.size()) return false;
        for (auto& [key, val] : a.items()) {
            if (!b.contains(key) || !JsonApproxEqual(val, b[key], epsilon)) return false;
        }
        return true;
    }
    return a == b; // Default comparison for integers, strings, booleans, etc.
}

namespace Editor {
    namespace {
        // Captures one entity and all descendants into snapshot list for full hierarchy restore
        void CollectEntitySnapshots(ECS::World* world, ECS::Entity entity, std::vector<EntitySnapshot>& out) {
            if (!world || entity.IsNull() || !world->IsAlive(entity)) {
                return;
            }

            EntitySnapshot snapshot;
            snapshot.Id = entity.Index;

            ECS::Entity parent = world->ParentOf(entity);
            snapshot.ParentId = parent.IsNull() ? ECS::Entity::NPOS32 : parent.Index;

            snapshot.Components = world->CaptureEntityComponents(entity);

            // Parent is restored separately to avoid stale hierarchy links in serialized component payloads
            const ECS::ComponentTypeId parentTypeId = ECS::TypeIdOf<ECS::Components::Parent>();
            snapshot.Components.erase(
                std::remove_if(snapshot.Components.begin(), snapshot.Components.end(),
                    [parentTypeId](const ECS::SerializedComponent& sc) { return sc.Id == parentTypeId; }),
                snapshot.Components.end()
            );

            out.push_back(std::move(snapshot));

            world->ForChildren(entity, [&](ECS::Entity child) {
                CollectEntitySnapshots(world, child, out);
            });
        }

        // Collects hierarchy ids in parent first order so callers can choose destroy or restore traversal direction
        void CollectEntityHierarchyIds(ECS::World* world, ECS::Entity entity, std::vector<EntityId>& out) {
            if (!world || entity.IsNull() || !world->IsAlive(entity)) {
                return;
            }

            out.push_back(entity.Index);

            world->ForChildren(entity, [&](ECS::Entity child) {
                CollectEntityHierarchyIds(world, child, out);
            });
        }

        // Destroys an entity tree from leaves upward to avoid parent child lifetime hazards
        void DestroyEntityHierarchy(ECS::World* world, EntityId entityId) {
            if (!world || entityId == ECS::Entity::NPOS32) {
                return;
            }

            ECS::Entity entity = world->Resolve(entityId);
            if (!world->IsAlive(entity)) {
                return;
            }

            std::vector<EntityId> entities;
            CollectEntityHierarchyIds(world, entity, entities);

            // Reverse traversal ensures children are destroyed before parent attachments are invalidated
            for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
                ECS::Entity current = world->Resolve(*it);
                if (world->IsAlive(current)) {
                    world->Destroy(current);
                }
            }
        }

        // Recreates entity hierarchy from snapshots by restoring entities first then parent links
        void RestoreEntityHierarchy(ECS::World* world, const std::vector<EntitySnapshot>& snapshots) {
            if (!world || snapshots.empty()) {
                return;
            }

            std::unordered_map<EntityId, ECS::Entity> entityMap;
            entityMap.reserve(snapshots.size());

            // Create all entity handles first so parent attach step can resolve full id map
            for (const auto& snapshot : snapshots) {
                ECS::Entity created = world->CreateWithId(snapshot.Id);
                entityMap.emplace(snapshot.Id, created);
            }

            // Applies serialized component set by replacing missing components and updating existing payload data
            auto applySnapshotComponents = [world](ECS::Entity entity,
                const std::vector<ECS::SerializedComponent>& comps,
                bool allowIfNotAlive) {
                if (!world) {
                    return;
                }

                if (!world->IsAlive(entity)) {
                    if (!allowIfNotAlive) {
                        return;
                    }

                    for (const auto& sc : comps) {
                        world->AddComponentById(entity, sc.Id,
                            const_cast<uint8_t*>(sc.Data.data()), sc.Data.size());
                    }
                    return;
                }

                std::unordered_set<ECS::ComponentTypeId> restoreIds;
                restoreIds.reserve(comps.size());
                for (const auto& sc : comps) {
                    restoreIds.insert(sc.Id);
                }

                auto existing = world->GetEntityComponents(entity);
                for (auto id : existing) {
                    if (restoreIds.find(id) == restoreIds.end()) {
                        world->RemoveById(entity, id);
                    }
                }

                for (const auto& sc : comps) {
                    void* ptr = world->GetRawComponentPtr(entity, sc.Id);
                    if (ptr) {
                        const auto& meta = ECS::ComponentRegistry::Meta(sc.Id);
                        const size_t copySize = meta.Size > 0
                            ? std::min(sc.Data.size(), meta.Size)
                            : sc.Data.size();
                        if (copySize > 0) {
                            std::memcpy(ptr, sc.Data.data(), copySize);
                        }
                    }
                    else {
                        world->AddComponentById(entity, sc.Id,
                            const_cast<uint8_t*>(sc.Data.data()), sc.Data.size());
                    }
                }
            };

            for (const auto& snapshot : snapshots) {
                auto it = entityMap.find(snapshot.Id);
                if (it == entityMap.end()) {
                    continue;
                }
                // Newly created entities are not alive until at least one component exists
                applySnapshotComponents(it->second, snapshot.Components, true);
            }

            for (const auto& snapshot : snapshots) {
                if (snapshot.ParentId == ECS::Entity::NPOS32) {
                    continue;
                }

                ECS::Entity child = world->Resolve(snapshot.Id);
                if (!world->IsAlive(child)) {
                    continue;
                }

                ECS::Entity parent = ECS::NULL_ENTITY;
                auto parentIt = entityMap.find(snapshot.ParentId);
                if (parentIt != entityMap.end()) {
                    parent = parentIt->second;
                }
                else {
                    parent = world->Resolve(snapshot.ParentId);
                }

                if (world->IsAlive(parent)) {
                    world->Attach(child, parent);
                }
            }
        }
    }

    // ========================================================================
    // TransformChangeCommand Implementation
    // ========================================================================

    // Stores before and after transform values for one entity so gizmo edits can be undone and redone
    TransformChangeCommand::TransformChangeCommand(
        ECS::World* world,
        ECS::Entity entity,
        const Vector3D& oldPos,
        const Quaternion& oldRot,
        const Vector3D& oldScale,
        const Vector3D& newPos,
        const Quaternion& newRot,
        const Vector3D& newScale
    )
        : m_world(world)
        , m_entity(entity)
        , m_oldPosition(oldPos)
        , m_oldRotation(oldRot)
        , m_oldScale(oldScale)
        , m_newPosition(newPos)
        , m_newRotation(newRot)
        , m_newScale(newScale)
    {
    }

    // Applies the recorded target transform values to the entity
    void TransformChangeCommand::Execute() {
        if (!m_world || !m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot execute transform change - entity invalid");
            return;
        }

        auto* transform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
            m_world, m_entity, "LocalTransform");
        if (!transform) {
            LOG_WARNING("[UndoSystem] Entity has no LocalTransform component");
            return;
        }

        // Write all transform channels in one step so position rotation and scale stay synchronized
        transform->Position = m_newPosition;
        transform->Rotation = m_newRotation;
        transform->Scale = m_newScale;

        LOG_DEBUG("[UndoSystem] Applied transform change to entity " << m_entity.Index);
    }

    // Restores the original transform values captured before the edit
    void TransformChangeCommand::Undo() {
        if (!m_world || !m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot undo transform change - entity invalid");
            return;
        }

        auto* transform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
            m_world, m_entity, "LocalTransform");
        if (!transform) {
            LOG_WARNING("[UndoSystem] Entity has no LocalTransform component");
            return;
        }

        transform->Position = m_oldPosition;
        transform->Rotation = m_oldRotation;
        transform->Scale = m_oldScale;

        LOG_DEBUG("[UndoSystem] Reverted transform change on entity " << m_entity.Index);
    }

    // ========================================================================
    // CreateEntityCommand Implementation
    // ========================================================================

    // Captures created entity hierarchy so undo can delete it and redo can reconstruct it exactly
    CreateEntityCommand::CreateEntityCommand(
        ECS::World* world,
        ECS::Entity entity,
        std::function<void()> onEntityDeleted
    )
        : m_world(world)
        , m_entityId(entity.Index)
        , m_onEntityDeleted(onEntityDeleted)
    {
        // Snapshot the full entity hierarchy at creation time for redo
        if (m_world && m_world->IsAlive(entity)) {
            CollectEntitySnapshots(m_world, entity, m_snapshots);
        }
    }

    // Redo path recreates the captured entity hierarchy
    void CreateEntityCommand::Execute() {
        // Execute = Restore the entity mainly for Redo
        if (!m_world || m_snapshots.empty()) {
            LOG_WARNING("[UndoSystem] Cannot execute entity creation - missing snapshots");
            return;
        }

        RestoreEntityHierarchy(m_world, m_snapshots);
        LOG_DEBUG("[UndoSystem] Entity " << m_entityId << " restored (redo creation)");
    }

    // Undo path removes the created hierarchy and triggers optional deletion callback
    void CreateEntityCommand::Undo() {
        // Undo creation deletes the current entity hierarchy
        if (!m_world || m_entityId == ECS::Entity::NPOS32) {
            LOG_WARNING("[UndoSystem] Cannot undo entity creation - entity invalid");
            return;
        }

        DestroyEntityHierarchy(m_world, m_entityId);

        if (m_onEntityDeleted) {
            m_onEntityDeleted();
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entityId << " deleted (undo creation)");
    }

    // ========================================================================
    // DeleteEntityCommand Implementation
    // ========================================================================

    // Captures entity hierarchy before deletion so undo can fully restore it
    DeleteEntityCommand::DeleteEntityCommand(
        ECS::World* world,
        ECS::Entity entity,
        std::function<void()> onEntityRestored
    )
        : m_world(world)
        , m_entityId(entity.Index)
        , m_onEntityRestored(onEntityRestored)
    {
        // Snapshot the full entity hierarchy before deletion for undo
        if (m_world && m_world->IsAlive(entity)) {
            CollectEntitySnapshots(m_world, entity, m_snapshots);
        }
    }

    // Execute path performs actual hierarchy deletion
    void DeleteEntityCommand::Execute() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32) {
            LOG_WARNING("[UndoSystem] Cannot execute entity deletion - entity invalid");
            return;
        }

        DestroyEntityHierarchy(m_world, m_entityId);
        LOG_DEBUG("[UndoSystem] Entity " << m_entityId << " deleted");
    }

    // Undo path rebuilds deleted hierarchy and triggers optional restore callback
    void DeleteEntityCommand::Undo() {
        if (!m_world || m_snapshots.empty()) {
            LOG_WARNING("[UndoSystem] Cannot undo entity deletion - missing snapshots");
            return;
        }

        RestoreEntityHierarchy(m_world, m_snapshots);

        if (m_onEntityRestored) {
            m_onEntityRestored();
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entityId << " restored (undo deletion)");
    }

    // ========================================================================
    // EntityComponentsSnapshotCommand Implementation
    // ========================================================================

    // Stores complete before and after component sets for one entity state transition
    EntityComponentsSnapshotCommand::EntityComponentsSnapshotCommand(
        ECS::World* world,
        ECS::Entity entity,
        std::vector<ECS::SerializedComponent> before,
        std::vector<ECS::SerializedComponent> after
    )
        : m_world(world)
        , m_entityId(entity.Index)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {
    }

    // Applies after snapshot by removing stale components and restoring target payload data
    void EntityComponentsSnapshotCommand::Execute() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32) {
            LOG_WARNING("[UndoSystem] Cannot execute component snapshot - entity invalid");
            return;
        }

        ECS::Entity entity = m_world->Resolve(m_entityId);
        if (entity.IsNull() || !m_world->IsAlive(entity)) {
            LOG_WARNING("[UndoSystem] Cannot execute component snapshot - entity invalid");
            return;
        }

        // Build fast membership set for components that should exist after execute
        std::unordered_set<ECS::ComponentTypeId> restoreIds;
        restoreIds.reserve(m_after.size());
        for (const auto& sc : m_after) {
            restoreIds.insert(sc.Id);
        }

        // Remove components not present in target snapshot so entity layout matches after state exactly
        auto existing = m_world->GetEntityComponents(entity);
        for (auto id : existing) {
            if (restoreIds.find(id) == restoreIds.end()) {
                m_world->RemoveById(entity, id);
            }
        }

        // Apply component payload data for all target components, creating missing ones when required
        for (const auto& sc : m_after) {
            void* ptr = m_world->GetRawComponentPtr(entity, sc.Id);
            if (ptr) {
                const auto& meta = ECS::ComponentRegistry::Meta(sc.Id);
                const size_t copySize = meta.Size > 0
                    ? std::min(sc.Data.size(), meta.Size)
                    : sc.Data.size();
                if (copySize > 0) {
                    std::memcpy(ptr, sc.Data.data(), copySize);
                }
            }
            else {
                m_world->AddComponentById(entity, sc.Id,
                    const_cast<uint8_t*>(sc.Data.data()), sc.Data.size());
            }
        }
    }

    // Restores before snapshot using the same replace style logic as execute
    void EntityComponentsSnapshotCommand::Undo() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32) {
            LOG_WARNING("[UndoSystem] Cannot undo component snapshot - entity invalid");
            return;
        }

        ECS::Entity entity = m_world->Resolve(m_entityId);
        if (entity.IsNull() || !m_world->IsAlive(entity)) {
            LOG_WARNING("[UndoSystem] Cannot undo component snapshot - entity invalid");
            return;
        }

        // Build membership set for components that should exist in pre edit state
        std::unordered_set<ECS::ComponentTypeId> restoreIds;
        restoreIds.reserve(m_before.size());
        for (const auto& sc : m_before) {
            restoreIds.insert(sc.Id);
        }

        // Remove components introduced by the edit that did not exist before
        auto existing = m_world->GetEntityComponents(entity);
        for (auto id : existing) {
            if (restoreIds.find(id) == restoreIds.end()) {
                m_world->RemoveById(entity, id);
            }
        }

        // Restore pre edit payload values and recreate removed components when needed
        for (const auto& sc : m_before) {
            void* ptr = m_world->GetRawComponentPtr(entity, sc.Id);
            if (ptr) {
                const auto& meta = ECS::ComponentRegistry::Meta(sc.Id);
                const size_t copySize = meta.Size > 0
                    ? std::min(sc.Data.size(), meta.Size)
                    : sc.Data.size();
                if (copySize > 0) {
                    std::memcpy(ptr, sc.Data.data(), copySize);
                }
            }
            else {
                m_world->AddComponentById(entity, sc.Id,
                    const_cast<uint8_t*>(sc.Data.data()), sc.Data.size());
            }
        }
    }

    // ========================================================================
    // ReorderEntitiesCommand Implementation
    // ========================================================================

    // Stores reorder callback and sibling ordering before and after a drag reorder operation
    ReorderEntitiesCommand::ReorderEntitiesCommand(
        EntityId parentId,
        std::function<void(const std::vector<EntityId>&)> applyOrder,
        std::vector<EntityId> before,
        std::vector<EntityId> after
    )
        : m_parentId(parentId)
        , m_applyOrder(std::move(applyOrder))
        , m_before(std::move(before))
        , m_after(std::move(after))
    {
    }

    // Applies reordered sibling list for redo path
    void ReorderEntitiesCommand::Execute() {
        if (m_applyOrder) {
            // Callback owns hierarchy reorder mechanics so command only supplies target order payload
            m_applyOrder(m_after);
        }
    }

    // Restores previous sibling ordering for undo path
    void ReorderEntitiesCommand::Undo() {
        if (m_applyOrder) {
            // Reuse same callback with before snapshot to restore original ordering
            m_applyOrder(m_before);
        }
    }

    // Updates pending after order when subsequent drags belong to same parent context
    bool ReorderEntitiesCommand::UpdateAfter(EntityId parentId, const std::vector<EntityId>& after) {
        if (parentId != m_parentId) {
            // Different parent means this drag sequence cannot be merged into current command
            return false;
        }

        // Replace only the after sequence so undo baseline remains original drag starting order
        m_after = after;
        return true;
    }

    // ========================================================================
    // ReparentEntityCommand Implementation
    // ========================================================================

    // Stores child and parent ids needed to apply and reverse one reparent operation
    ReparentEntityCommand::ReparentEntityCommand(
        ECS::World* world,
        EntityId childId,
        EntityId oldParentId,
        EntityId newParentId
    )
        : m_world(world)
        , m_childId(childId)
        , m_oldParentId(oldParentId)
        , m_newParentId(newParentId)
    {
    }

    // Applies parent assignment or root detach depending on provided parent id sentinel
    void ReparentEntityCommand::ApplyParent(EntityId parentId) {
        // Bail if world is gone or child ID was never set
        if (!m_world || m_childId == ECS::Entity::NPOS32) {
            return;
        }

        // Resolve the child entity and verify it still exists in the world
        ECS::Entity child = m_world->Resolve(m_childId);
        if (child.IsNull() || !m_world->IsAlive(child)) {
            return;
        }

        if (parentId == ECS::Entity::NPOS32) {
            // NPOS32 means no parent, so promote child to a root entity
            m_world->Detach(child);
            return;
        }

        // Resolve the target parent and verify it still exists before attaching
        ECS::Entity parent = m_world->Resolve(parentId);
        if (parent.IsNull() || !m_world->IsAlive(parent)) {
            return;
        }

        // Attach the child under the resolved parent, updating hierarchy indices internally
        m_world->Attach(child, parent);
    }

    // Moves child under new parent id for redo and first execute
    void ReparentEntityCommand::Execute() {
        ApplyParent(m_newParentId);
    }

    // Restores child original parent for undo path
    void ReparentEntityCommand::Undo() {
        ApplyParent(m_oldParentId);
    }

    // ========================================================================
    // ComponentPropertyCommand Implementation
    // ========================================================================

    /**
    * @brief Constructs a command representing a component property change.
    *
    * Stores the entity, component, property path, and both the old and new values.
    * These values are later used for undo and redo operations.
    *
    * @param world Pointer to the ECS world.
    * @param entityId ID of the entity whose property is modified.
    * @param componentId Identifier of the component containing the property.
    * @param propertyPath Dot-separated path to the property.
    * @param oldValue Original value of the property.
    * @param newValue Updated value of the property.
    * @param applyFn Function used to apply the property value.
    */
    ComponentPropertyCommand::ComponentPropertyCommand(
        ECS::World* world,
        EntityId entityId,
        ECS::ComponentTypeId componentId,
        std::string propertyPath,
        nlohmann::json oldValue,
        nlohmann::json newValue,
        ApplyFn applyFn
    )
        : m_world(world)
        , m_entityId(entityId)
        , m_componentId(componentId)
        , m_propertyPath(std::move(propertyPath))
        , m_oldValue(std::move(oldValue))
        , m_newValue(std::move(newValue))
        , m_applyFn(std::move(applyFn))
    {
    }

    // Writes new property value into component through injected apply function
    void ComponentPropertyCommand::Execute() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32 || !m_applyFn) return;

        // Resolve from stable id each call so undo remains valid even after handle churn
        ECS::Entity e = m_world->Resolve(m_entityId);
        if (!m_world->IsAlive(e)) return;
        m_applyFn(m_world, e, m_componentId, m_propertyPath, m_newValue);
    }

    // Writes previous property value back into component through same apply path
    void ComponentPropertyCommand::Undo() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32 || !m_applyFn) return;

        // Use identical write path as execute so serialization and side effects remain consistent
        ECS::Entity e = m_world->Resolve(m_entityId);
        if (!m_world->IsAlive(e)) return;
        m_applyFn(m_world, e, m_componentId, m_propertyPath, m_oldValue);
    }

    // Merges sequential edits on the same property by keeping old value and replacing only latest new value
    bool ComponentPropertyCommand::Coalesce(ICommand* other) {
        auto* o = dynamic_cast<ComponentPropertyCommand*>(other);
        if (!o) return false;

        // Commands must target the exact same world entity component and property path to be mergeable
        if (m_world != o->m_world) return false;
        if (m_entityId != o->m_entityId) return false;
        if (m_componentId != o->m_componentId) return false;
        if (m_propertyPath != o->m_propertyPath) return false;

        // Keep original old value and promote latest new value to collapse drag noise
        m_newValue = o->m_newValue;
        return true;
    }

    // ========================================================================
    // BatchComponentPropertyCommand Implementation
    // ========================================================================
    
    /**
    * @brief Constructs a batch command representing property changes
    * for multiple entities.
    *
    * @param world Pointer to the ECS world.
    * @param componentId Identifier of the component being modified.
    * @param propertyPath Dot-separated path to the property.
    * @param entries Collection of entity property changes.
    * @param applyFn Function used to apply property values.
    */
    BatchComponentPropertyCommand::BatchComponentPropertyCommand(
        ECS::World* world,
        ECS::ComponentTypeId componentId,
        std::string propertyPath,
        std::vector<Entry> entries,
        ApplyFn applyFn
    )
        : m_world(world)
        , m_componentId(componentId)
        , m_propertyPath(std::move(propertyPath))
        , m_entries(std::move(entries))
        , m_applyFn(std::move(applyFn))
    {
    }

    /**
    * @brief Applies the new property value to all entities in the batch.
    */
    void BatchComponentPropertyCommand::Execute() {
        if (!m_world || !m_applyFn) return;
        for (const auto& it : m_entries) { // Apply the new value for each entity
            ECS::Entity e = m_world->Resolve(it.Entity);
            if (!m_world->IsAlive(e)) continue;
            m_applyFn(m_world, e, m_componentId, m_propertyPath, it.NewValue);
        }
    }

    /**
    * @brief Restores the original property values for all entities in the batch.
    */
    void BatchComponentPropertyCommand::Undo() {
        if (!m_world || !m_applyFn) return;
        // Restore the old value for each entity
        for (const auto& it : m_entries) {
            ECS::Entity e = m_world->Resolve(it.Entity);
            if (!m_world->IsAlive(e)) continue;
            m_applyFn(m_world, e, m_componentId, m_propertyPath, it.OldValue);
        }
    }

    /**
    * @brief Attempts to merge two batch commands.
    *
    * If both commands modify the same component property for the same
    * set of entities, the latest values are merged into this command.
    *
    * @param other Another command to attempt merging with.
    * @return True if the commands were successfully merged.
    */
    bool BatchComponentPropertyCommand::Coalesce(ICommand* other) {
        auto* o = dynamic_cast<BatchComponentPropertyCommand*>(other);
        if (!o) return false;

        // Merge only when both commands describe the same batch target configuration
        if (m_world != o->m_world) return false;
        if (m_componentId != o->m_componentId) return false;
        if (m_propertyPath != o->m_propertyPath) return false;
        if (m_entries.size() != o->m_entries.size()) return false;

        // Preserve entity order contract so each entry old and new values map to same target entity
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].Entity != o->m_entries[i].Entity) return false;
        }

        // Keep initial old values and refresh only new values from latest command
        for (size_t i = 0; i < m_entries.size(); ++i) {
            m_entries[i].NewValue = o->m_entries[i].NewValue;
        }
        return true;
    }

    // ========================================================================
    // TilePaintCommand Implementation
    // ========================================================================

    // Captures one tile write with before and after values plus optional change callback
    TilePaintCommand::TilePaintCommand(
        std::shared_ptr<TileMap> map,
        int32_t x, int32_t y,
        uint32_t oldTile,
        uint32_t newTile,
        std::function<void(int32_t, int32_t, uint32_t)> onTileChanged
    )
        : m_map(std::move(map))  // Move shared pointer and callback to avoid extra refcount churn
        , m_x(x)
        , m_y(y)
        , m_oldTile(oldTile)
        , m_newTile(newTile)
        , m_onTileChanged(std::move(onTileChanged))
    {
    }

    // Applies new packed tile value and notifies listeners so scene state can refresh
    void TilePaintCommand::Execute() {
        // Execute is the redo path for this command
        if (!m_map) return;

        // Write new packed tile id into layer zero
        m_map->SetTileSigned(0, m_x, m_y, m_newTile);

        // Callback lets caller mark scene dirty and trigger save integration
        if (m_onTileChanged) {
            m_onTileChanged(m_x, m_y, m_newTile);
        }
    }

    // Restores old packed tile value and emits same callback path
    void TilePaintCommand::Undo() {
        // Undo restores tile state captured before execute
        if (!m_map) return;

        // Write old packed tile id back into layer zero
        m_map->SetTileSigned(0, m_x, m_y, m_oldTile);

        // Reuse callback path so dependent systems react the same way as execute
        if (m_onTileChanged) {
            m_onTileChanged(m_x, m_y, m_oldTile);
        }
    }

    // ========================================================================
    // TileCollisionPaintCommand Implementation
    // ========================================================================

    // Stores tile coordinates plus before and after collision masks and scene change callback
    TileCollisionPaintCommand::TileCollisionPaintCommand(
        std::shared_ptr<TileMap> map,
        int32_t x, int32_t y,
        uint8_t oldMask,
        uint8_t newMask,
        std::function<void(int32_t, int32_t, uint8_t)> onCollisionChanged
    )
        : m_map(std::move(map))
        , m_x(x)
        , m_y(y)
        , m_oldMask(oldMask)
        , m_newMask(newMask)
        , m_onCollisionChanged(std::move(onCollisionChanged))
    {
    }

    // Applies new collision mask and emits callback so editor dirty state can update
    void TileCollisionPaintCommand::Execute() {
        if (!m_map) return;
        m_map->SetCollisionMaskSigned(m_x, m_y, m_newMask);
        if (m_onCollisionChanged) {
            m_onCollisionChanged(m_x, m_y, m_newMask); // Notify listeners that collision data changed
        }
    }

    // Restores previous collision mask and emits callback so save state stays accurate
    void TileCollisionPaintCommand::Undo() {
        if (!m_map) return;
        m_map->SetCollisionMaskSigned(m_x, m_y, m_oldMask);
        if (m_onCollisionChanged) {
            m_onCollisionChanged(m_x, m_y, m_oldMask); // Notify listeners that collision data was reverted
        }
    }

    // ========================================================================
    // UndoSystem Implementation
    // ========================================================================

    // Initializes world binding and enforces a minimum undo history size
    void UndoSystem::Initialize(ECS::World* world, size_t maxStackSize) {
        m_world = world;
        m_maxStackSize = std::max(size_t(20), maxStackSize);
        LOG_INFO("[UndoSystem] Initialized with max stack size: " << m_maxStackSize);
    }

    // Polls keyboard shortcuts and dispatches undo or redo when text input is not active
    void UndoSystem::Update() {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput) {
            return;
        }

        // Check for CTRL+Z (Undo)
        if ((Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL)) &&
            Input::IsKeyPressed(KEY_Z)) {
            LOG_DEBUG("[UndoSystem] CTRL+Z detected, attempting undo...");
            bool success = Undo();
            if (success) {
                LOG_INFO("[UndoSystem] Undo successful");
            }
            else {
                LOG_DEBUG("[UndoSystem] Undo failed - nothing to undo");
            }
        }

        // Check for CTRL+Y (Redo)
        if ((Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL)) &&
            Input::IsKeyPressed(KEY_Y)) {
            LOG_DEBUG("[UndoSystem] CTRL+Y detected, attempting redo...");
            bool success = Redo();
            if (success) {
                LOG_INFO("[UndoSystem] Redo successful");
            }
            else {
                LOG_DEBUG("[UndoSystem] Redo failed - nothing to redo");
            }
        }
    }

    // Executes command coalesces compatible edits and manages undo and redo stacks
    void UndoSystem::ExecuteCommand(std::unique_ptr<ICommand> command) {
        if (!command) {
            LOG_WARNING("[UndoSystem] Attempted to execute null command");
            return;
        }

        command->Execute();

        // Emit property edit flag for inspector refresh flows that listen to aggregate property commits
        if (dynamic_cast<ComponentPropertyCommand*>(command.get()) != nullptr ||
            dynamic_cast<BatchComponentPropertyCommand*>(command.get()) != nullptr) {
            m_propertyEditEmitted = true; // Mark that a property edit has occurred
        }

        // Coalesce with stack tail to merge drag style edits into one undo entry
        if (!m_undoStack.empty()) {
            auto* tail = m_undoStack.back().get();
            if (tail && tail->Coalesce(command.get())) {
                // Clear redo history since a new command modifies the timeline
                m_redoStack.clear();
                TrimUndoStack(); // Ensure undo stack does not exceed its maximum size
                return;
            }
        }

        m_undoStack.push_back(std::move(command));

        // New executed command invalidates redo history by definition
        m_redoStack.clear();

        TrimUndoStack();

        LOG_DEBUG("[UndoSystem] Command executed. Undo stack size: " << m_undoStack.size());
    }

    // Undoes most recent command and moves it to redo stack
    bool UndoSystem::Undo() {
        if (m_undoStack.empty()) {
            LOG_DEBUG("[UndoSystem] Nothing to undo");
            return false;
        }

        // Get the last command
        // Get latest command from undo stack tail and transfer ownership
        auto command = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        // Undo it
        command->Undo();

        // Move to redo stack
        // Store undone command in redo stack so redo can reapply it
        m_redoStack.push_back(std::move(command));

        LOG_INFO("[UndoSystem] Undo performed. Undo stack: " << m_undoStack.size()
            << ", Redo stack: " << m_redoStack.size());

        return true;
    }

    // Redoes most recently undone command and moves it back to undo stack
    bool UndoSystem::Redo() {
        if (m_redoStack.empty()) {
            LOG_DEBUG("[UndoSystem] Nothing to redo");
            return false;
        }

        // Get the last undone command
        // Get latest command from redo stack tail and transfer ownership
        auto command = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        // Redo it
        command->Redo();

        // Move back to undo stack
        // Return command to undo stack because it is now active again
        m_undoStack.push_back(std::move(command));

        LOG_INFO("[UndoSystem] Redo performed. Undo stack: " << m_undoStack.size()
            << ", Redo stack: " << m_redoStack.size());

        return true;
    }

    // Clears all undo and redo history entries
    void UndoSystem::Clear() {
        // Drop both stacks to reset editor command history immediately
        m_undoStack.clear();
        m_redoStack.clear();
        LOG_INFO("[UndoSystem] Undo/redo history cleared");
    }

    // Trims oldest undo entries when stack exceeds configured limit
    void UndoSystem::TrimUndoStack() {
        // Pop from front because deque front contains oldest commands
        while (m_undoStack.size() > m_maxStackSize) {
            m_undoStack.pop_front();
        }
    }

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    // Records a transform command from provided before and after state snapshots
    void UndoSystem::RecordTransformChange(
        EntityId entityId,
        const Vector3D& oldPos, const Quaternion& oldRot, const Vector3D& oldScale,
        const Vector3D& newPos, const Quaternion& newRot, const Vector3D& newScale
    ) {
        if (!m_world) {
            LOG_WARNING("[UndoSystem] Cannot record transform change - world not initialized");
            return;
        }

        ECS::Entity entity = m_world->Resolve(entityId);

        if (!m_world->IsAlive(entity)) {
            LOG_WARNING("[UndoSystem] Cannot record transform change - entity is invalid");
            return;
        }

        auto command = std::make_unique<TransformChangeCommand>(
            m_world, entity,
            oldPos, oldRot, oldScale,
            newPos, newRot, newScale
        );

        // Push direct command then clear redo just like ExecuteCommand without immediate execution
        m_undoStack.push_back(std::move(command));
        m_redoStack.clear();
        TrimUndoStack();
    }

    // Records entity creation command for later undo and redo operations
    void UndoSystem::RecordEntityCreation(EntityId entityId) {
        if (!m_world) {
            LOG_WARNING("[UndoSystem] Cannot record entity creation - world not initialized");
            return;
        }

        ECS::Entity entity = m_world->Resolve(entityId);

        if (!m_world->IsAlive(entity)) {
            LOG_WARNING("[UndoSystem] Cannot record entity creation - entity is invalid");
            return;
        }

        auto command = std::make_unique<CreateEntityCommand>(m_world, entity);

        m_undoStack.push_back(std::move(command));
        m_redoStack.clear();
        TrimUndoStack();

        LOG_DEBUG("[UndoSystem] Entity creation recorded. Undo stack size: " << m_undoStack.size());
    }

    // Records entity deletion command for later undo and redo operations
    void UndoSystem::RecordEntityDeletion(EntityId entityId) {
        if (!m_world) {
            LOG_WARNING("[UndoSystem] Cannot record entity deletion - world not initialized");
            return;
        }

        ECS::Entity entity = m_world->Resolve(entityId);

        if (!m_world->IsAlive(entity)) {
            LOG_WARNING("[UndoSystem] Cannot record entity deletion - entity is invalid");
            return;
        }

        auto command = std::make_unique<DeleteEntityCommand>(m_world, entity);

        m_undoStack.push_back(std::move(command));
        m_redoStack.clear();
        TrimUndoStack();

        LOG_DEBUG("[UndoSystem] Entity deletion recorded. Undo stack size: " << m_undoStack.size());
    }

    // Records parent change command so hierarchy drag and drop can be undone and redone
    void UndoSystem::RecordEntityReparent(EntityId childId, EntityId oldParentId, EntityId newParentId) {
        // World must be set before any undo recording can happen
        if (!m_world) {
            LOG_WARNING("[UndoSystem] Cannot record entity reparent - world not initialized");
            return;
        }

        // Skip if child is invalid or the parent didn't actually change
        if (childId == ECS::Entity::NPOS32 || oldParentId == newParentId) {
            return;
        }

        // Verify the child entity is still alive before recording
        ECS::Entity child = m_world->Resolve(childId);
        if (child.IsNull() || !m_world->IsAlive(child)) {
            LOG_WARNING("[UndoSystem] Cannot record entity reparent - child is invalid");
            return;
        }

        // Build the command with the before/after parent IDs so it can restore either direction
        auto command = std::make_unique<ReparentEntityCommand>(m_world, childId, oldParentId, newParentId);
        m_undoStack.push_back(std::move(command));

        // Any new action invalidates the redo history
        m_redoStack.clear();

        // Keep the undo stack within its configured size limit
        TrimUndoStack();

        LOG_DEBUG("[UndoSystem] Entity reparent recorded. Undo stack size: " << m_undoStack.size());
    }

    // Attempts to merge latest reorder command by updating its after sequence
    bool UndoSystem::CoalesceReorder(EntityId parentId, const std::vector<EntityId>& after) {
        if (m_undoStack.empty()) {
            return false;
        }

        // Only latest command can be coalesced because undo ordering must stay stable
        auto* command = dynamic_cast<ReorderEntitiesCommand*>(m_undoStack.back().get());
        if (!command) {
            return false;
        }

        if (!command->UpdateAfter(parentId, after)) {
            return false;
        }

        // Merged reorder is a new user action so redo history must be dropped
        m_redoStack.clear();
        return true;
    }

    // Starts tracking a property edit by storing its original value on first begin call
    void UndoSystem::BeginPropertyEdit(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath, const nlohmann::json& oldValue) {
        PropertyKey key{ entityId, componentId, propertyPath };
        if (m_activePropertyEdits.find(key) == m_activePropertyEdits.end()) {
            m_activePropertyEdits.emplace(std::move(key), oldValue);
        }
    }

    // Finishes a tracked property edit and emits undo command only when value actually changed
    void UndoSystem::EndPropertyEdit(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath, const nlohmann::json& newValue,
        ComponentPropertyCommand::ApplyFn applyFn) {
        PropertyKey key{ entityId, componentId, propertyPath };

        // Missing key means begin edit was never registered or already consumed
        auto it = m_activePropertyEdits.find(key);
        if (it == m_activePropertyEdits.end()) {
            return;
        }

        const nlohmann::json& oldVal = it->second;
        if (JsonApproxEqual(oldVal, newValue)) {
            // Remove tracked key on no op so stale state does not accumulate
            m_activePropertyEdits.erase(it);
            return;
        }
        if (!m_world) {
            // World missing means command cannot run, still clear edit tracking key
            m_activePropertyEdits.erase(it);
            return;
        }
        auto cmd = std::make_unique<ComponentPropertyCommand>(
            m_world, entityId, componentId, propertyPath, oldVal, newValue, std::move(applyFn)
        );
        ExecuteCommand(std::move(cmd));
        m_activePropertyEdits.erase(it);
    }

    /**
    * @brief Begins a batch property edit for multiple entities.
    * Captures the current value of a specific component property for each entity
    * so it can be used as the "before" state when generating undo commands.
    *
    * @param entities Set of entity IDs whose component property will be edited.
    * @param componentId Identifier of the component containing the property.
    * @param propertyPath Dot-separated path to the property inside the component (e.g. "Position.X").
    */
    void UndoSystem::BeginBatchPropertyEdit(const std::unordered_set<EntityId>& entities, ECS::ComponentTypeId componentId, const std::string& propertyPath) {
        if (!m_world) return; // Ensure the world context exists before continuing

        // Resolve component short name from registry so we can locate matching component JSON in serialization
        const auto& metaInfo = ECS::ComponentRegistry::Meta(componentId);
        const std::string compShortName = ECS::ComponentRegistry::GetComponentNameFromHash(metaInfo.TypeHash);
        if (compShortName.empty()) { // If the component name cannot be resolved, abort
            return;
        }

        // Capture each entity current property value as before state for batch undo
        for (EntityId id : entities) {
            ECS::Entity entity = m_world->Resolve(id); // Resolve the runtime entity handle from its ID
            if (!m_world->IsAlive(entity)) continue; // Skip entities that are no longer alive

            // Serialize entity once so path extraction uses consistent data shape with inspector serializer
            nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
            nlohmann::json* componentData = nullptr;

            // Locate target component payload by typename and keep pointer to its Data object
            if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
                for (auto& comp : entityJson["Components"]) {
                    // Ensure the component entry contains a valid type name
                    if (!comp.contains("TypeName") || !comp["TypeName"].is_string()) continue;
                    std::string typeName = comp["TypeName"];
                    // Match either the short name or fully-qualified name
                    if (typeName == compShortName || typeName == "ECS::Components::" + compShortName) {
                        // Retrieve the component's data block
                        if (comp.contains("Data") && comp["Data"].is_object()) {
                            componentData = &comp["Data"];
                        }
                        break;
                    }
                }
            }

            nlohmann::json propertyVal;

            if (componentData) {
                // Extract the specific property from the component JSON
                // propertyPath is like "Position.X"
                propertyVal = *componentData; // Start from the component's JSON data
                size_t start = 0;
                size_t end = propertyPath.find('.');
                while (end != std::string::npos) {
                    std::string part = propertyPath.substr(start, end - start);
                    // Traverse the JSON hierarchy
                    if (propertyVal.is_object() && propertyVal.contains(part)) {
                        propertyVal = propertyVal[part];
                    }
                    else {
                        propertyVal = nlohmann::json();
                        break;
                    }
                    start = end + 1;
                    end = propertyPath.find('.', start);
                }

                // Resolve final token after loop and clear value when path does not exist
                if (!propertyVal.is_null()) {
                    std::string lastPart = propertyPath.substr(start);
                    if (propertyVal.is_object() && propertyVal.contains(lastPart)) {
                        propertyVal = propertyVal[lastPart];
                    }
                    else {
                        propertyVal = nlohmann::json();
                    }
                }
            }
            else {
                propertyVal = nlohmann::json();
            }

            // Reuse single property tracking map to store each entity before value
            BeginPropertyEdit(id, componentId, propertyPath, propertyVal);
        }
    }

    // Ends a multi entity property edit by creating one batch command for all changed entities
    void UndoSystem::EndBatchPropertyEdit(const std::unordered_set<EntityId>& entities, ECS::ComponentTypeId componentId, const std::string& propertyPath, const nlohmann::json& newValue,
        ComponentPropertyCommand::ApplyFn applyFn) {
        if (!m_world || !applyFn) return; // Ensure required systems and callbacks are valid

        std::vector<BatchComponentPropertyCommand::Entry> entries;

        // Collect changed entries only so no op entities do not create unnecessary undo payload data
        for (EntityId id : entities) {
            PropertyKey key{ id, componentId, propertyPath };
            auto it = m_activePropertyEdits.find(key);
            if (it == m_activePropertyEdits.end()) continue;

            const nlohmann::json& oldVal = it->second;
            // Skip recording if the value did not change
            if (JsonApproxEqual(oldVal, newValue)) {
                m_activePropertyEdits.erase(it);
                continue;
            }

            entries.push_back({ id, oldVal, newValue }); // Store the change entry
            m_activePropertyEdits.erase(it); // Remove the active edit record
        }

        // Emit one batch command only when at least one entity changed
        if (!entries.empty()) {
            auto cmd = std::make_unique<BatchComponentPropertyCommand>(
                m_world, componentId, propertyPath, std::move(entries), std::move(applyFn)
            );
            ExecuteCommand(std::move(cmd));
        }
    }

    /**
    * @brief Records a property change for a single entity.
    * Generates an undoable command if the old and new values differ.
    *
    * @param entityId ID of the entity whose property changed.
    * @param componentId Identifier of the component containing the property.
    * @param propertyPath Dot-separated path to the property.
    * @param oldValue Previous value of the property.
    * @param newValue Updated value of the property.
    * @param applyFn Function used to apply the property during undo/redo.
    */
    void UndoSystem::RecordPropertyChange(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath,
        const nlohmann::json& oldValue, const nlohmann::json& newValue, ComponentPropertyCommand::ApplyFn applyFn) {
        if (!m_world) return; // Ensure the world exists
        if (JsonApproxEqual(oldValue, newValue)) return; // Ignore if the value has not changed
        auto cmd = std::make_unique<ComponentPropertyCommand>( // Create a command representing this property modification
            m_world, entityId, componentId, propertyPath, oldValue, newValue, std::move(applyFn)
        );
        ExecuteCommand(std::move(cmd)); // Execute and register the command with the undo system
    }

    /**
    * @brief Consumes the property edit emission flag.
    * Returns whether a property edit event was emitted and resets the flag.
    *
    * @return True if a property edit event occurred since the last call.
    */
    bool UndoSystem::ConsumePropertyEditEmission() {
        // Consume semantics ensure callers observe each emitted flag transition only once
        bool was = m_propertyEditEmitted;
        m_propertyEditEmitted = false;     // Reset the emission flag
        return was;
    }

} // namespace Editor
