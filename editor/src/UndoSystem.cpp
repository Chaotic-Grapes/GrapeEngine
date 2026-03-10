/* Start Header *****************************************************************/
/*!
\file   UndoSystem.cpp
\author Samantha Leong Sher Yen
\par    s.leong@digipen.edu
\date   21th January 2026
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

static bool JsonApproxEqual(const nlohmann::json& a, const nlohmann::json& b, float epsilon = 1e-5f) {
    if (a.type() != b.type()) return false;
    if (a.is_number_float()) {
        return std::abs(a.get<float>() - b.get<float>()) < epsilon;
    }
    if (a.is_array()) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) {
            if (!JsonApproxEqual(a[i], b[i], epsilon)) return false;
        }
        return true;
    }
    if (a.is_object()) {
        if (a.size() != b.size()) return false;
        for (auto& [key, val] : a.items()) {
            if (!b.contains(key) || !JsonApproxEqual(val, b[key], epsilon)) return false;
        }
        return true;
    }
    return a == b;
}

namespace Editor {
    namespace {
        void CollectEntitySnapshots(ECS::World* world, ECS::Entity entity, std::vector<EntitySnapshot>& out) {
            if (!world || entity.IsNull() || !world->IsAlive(entity)) {
                return;
            }

            EntitySnapshot snapshot;
            snapshot.Id = entity.Index;

            ECS::Entity parent = world->ParentOf(entity);
            snapshot.ParentId = parent.IsNull() ? ECS::Entity::NPOS32 : parent.Index;

            snapshot.Components = world->CaptureEntityComponents(entity);

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

        void CollectEntityHierarchyIds(ECS::World* world, ECS::Entity entity, std::vector<EntityId>& out) {
            if (!world || entity.IsNull() || !world->IsAlive(entity)) {
                return;
            }

            out.push_back(entity.Index);

            world->ForChildren(entity, [&](ECS::Entity child) {
                CollectEntityHierarchyIds(world, child, out);
            });
        }

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

            for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
                ECS::Entity current = world->Resolve(*it);
                if (world->IsAlive(current)) {
                    world->Destroy(current);
                }
            }
        }

        void RestoreEntityHierarchy(ECS::World* world, const std::vector<EntitySnapshot>& snapshots) {
            if (!world || snapshots.empty()) {
                return;
            }

            std::unordered_map<EntityId, ECS::Entity> entityMap;
            entityMap.reserve(snapshots.size());

            for (const auto& snapshot : snapshots) {
                ECS::Entity created = world->CreateWithId(snapshot.Id);
                entityMap.emplace(snapshot.Id, created);
            }

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
                    } else {
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
                // Newly created entities are not "alive" until they get components.
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
                } else {
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

        transform->Position = m_newPosition;
        transform->Rotation = m_newRotation;
        transform->Scale = m_newScale;

        LOG_DEBUG("[UndoSystem] Applied transform change to entity " << m_entity.Index);
    }

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

    CreateEntityCommand::CreateEntityCommand(
        ECS::World* world,
        ECS::Entity entity,
        std::function<void()> onEntityDeleted
    )
        : m_world(world)
        , m_entityId(entity.Index)
        , m_onEntityDeleted(onEntityDeleted)
    {
        // Snapshot the full entity hierarchy at creation time for redo.
        if (m_world && m_world->IsAlive(entity)) {
            CollectEntitySnapshots(m_world, entity, m_snapshots);
        }
    }

    void CreateEntityCommand::Execute() {
        // Execute = Restore the entity mainly for Redo
        if (!m_world || m_snapshots.empty()) {
            LOG_WARNING("[UndoSystem] Cannot execute entity creation - missing snapshots");
            return;
        }

        RestoreEntityHierarchy(m_world, m_snapshots);
        LOG_DEBUG("[UndoSystem] Entity " << m_entityId << " restored (redo creation)");
    }

    void CreateEntityCommand::Undo() {
        // Undo creation = delete the current entity hierarchy.
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

    DeleteEntityCommand::DeleteEntityCommand(
        ECS::World* world,
        ECS::Entity entity,
        std::function<void()> onEntityRestored
    )
        : m_world(world)
        , m_entityId(entity.Index)
        , m_onEntityRestored(onEntityRestored)
    {
        // Snapshot the full entity hierarchy before deletion for undo.
        if (m_world && m_world->IsAlive(entity)) {
            CollectEntitySnapshots(m_world, entity, m_snapshots);
        }
    }

    void DeleteEntityCommand::Execute() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32) {
            LOG_WARNING("[UndoSystem] Cannot execute entity deletion - entity invalid");
            return;
        }

        DestroyEntityHierarchy(m_world, m_entityId);
        LOG_DEBUG("[UndoSystem] Entity " << m_entityId << " deleted");
    }

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

        std::unordered_set<ECS::ComponentTypeId> restoreIds;
        restoreIds.reserve(m_after.size());
        for (const auto& sc : m_after) {
            restoreIds.insert(sc.Id);
        }

        auto existing = m_world->GetEntityComponents(entity);
        for (auto id : existing) {
            if (restoreIds.find(id) == restoreIds.end()) {
                m_world->RemoveById(entity, id);
            }
        }

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
            } else {
                m_world->AddComponentById(entity, sc.Id,
                    const_cast<uint8_t*>(sc.Data.data()), sc.Data.size());
            }
        }
    }

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

        std::unordered_set<ECS::ComponentTypeId> restoreIds;
        restoreIds.reserve(m_before.size());
        for (const auto& sc : m_before) {
            restoreIds.insert(sc.Id);
        }

        auto existing = m_world->GetEntityComponents(entity);
        for (auto id : existing) {
            if (restoreIds.find(id) == restoreIds.end()) {
                m_world->RemoveById(entity, id);
            }
        }

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
            } else {
                m_world->AddComponentById(entity, sc.Id,
                    const_cast<uint8_t*>(sc.Data.data()), sc.Data.size());
            }
        }
    }

    // ========================================================================
    // ReorderEntitiesCommand Implementation
    // ========================================================================

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

    void ReorderEntitiesCommand::Execute() {
        if (m_applyOrder) {
            m_applyOrder(m_after);
        }
    }

    void ReorderEntitiesCommand::Undo() {
        if (m_applyOrder) {
            m_applyOrder(m_before);
        }
    }

    bool ReorderEntitiesCommand::UpdateAfter(EntityId parentId, const std::vector<EntityId>& after) {
        if (parentId != m_parentId) {
            return false;
        }
        m_after = after;
        return true;
    }

    // ========================================================================
    // ReparentEntityCommand Implementation
    // ========================================================================

    // Store all IDs needed to apply and reverse the reparent operation
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

    // Shared logic for both Execute and Undo, attaches child to parentId or detaches if parentId is NPOS32
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

    // Applies the reparent by moving the child under newParentId
    void ReparentEntityCommand::Execute() {
        ApplyParent(m_newParentId);
    }

    // Reverses the reparent by restoring the child to oldParentId
    void ReparentEntityCommand::Undo() {
        ApplyParent(m_oldParentId);
    }

    // ========================================================================
    // ComponentPropertyCommand Implementation
    // ========================================================================

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

    void ComponentPropertyCommand::Execute() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32 || !m_applyFn) return;
        ECS::Entity e = m_world->Resolve(m_entityId);
        if (!m_world->IsAlive(e)) return;
        m_applyFn(m_world, e, m_componentId, m_propertyPath, m_newValue);
    }

    void ComponentPropertyCommand::Undo() {
        if (!m_world || m_entityId == ECS::Entity::NPOS32 || !m_applyFn) return;
        ECS::Entity e = m_world->Resolve(m_entityId);
        if (!m_world->IsAlive(e)) return;
        m_applyFn(m_world, e, m_componentId, m_propertyPath, m_oldValue);
    }

    bool ComponentPropertyCommand::Coalesce(ICommand* other) {
        auto* o = dynamic_cast<ComponentPropertyCommand*>(other);
        if (!o) return false;
        if (m_world != o->m_world) return false;
        if (m_entityId != o->m_entityId) return false;
        if (m_componentId != o->m_componentId) return false;
        if (m_propertyPath != o->m_propertyPath) return false;
        m_newValue = o->m_newValue;
        return true;
    }

    // ========================================================================
    // BatchComponentPropertyCommand Implementation
    // ========================================================================

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

    void BatchComponentPropertyCommand::Execute() {
        if (!m_world || !m_applyFn) return;
        for (const auto& it : m_entries) {
            ECS::Entity e = m_world->Resolve(it.Entity);
            if (!m_world->IsAlive(e)) continue;
            m_applyFn(m_world, e, m_componentId, m_propertyPath, it.NewValue);
        }
    }

    void BatchComponentPropertyCommand::Undo() {
        if (!m_world || !m_applyFn) return;
        for (const auto& it : m_entries) {
            ECS::Entity e = m_world->Resolve(it.Entity);
            if (!m_world->IsAlive(e)) continue;
            m_applyFn(m_world, e, m_componentId, m_propertyPath, it.OldValue);
        }
    }

    bool BatchComponentPropertyCommand::Coalesce(ICommand* other) {
        auto* o = dynamic_cast<BatchComponentPropertyCommand*>(other);
        if (!o) return false;
        if (m_world != o->m_world) return false;
        if (m_componentId != o->m_componentId) return false;
        if (m_propertyPath != o->m_propertyPath) return false;
        if (m_entries.size() != o->m_entries.size()) return false;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].Entity != o->m_entries[i].Entity) return false;
        }
        for (size_t i = 0; i < m_entries.size(); ++i) {
            m_entries[i].NewValue = o->m_entries[i].NewValue;
        }
        return true;
    }

    // ========================================================================
    // TilePaintCommand Implementation
    // ========================================================================

    TilePaintCommand::TilePaintCommand(
        std::shared_ptr<TileMap> map,
        int32_t x, int32_t y,
        uint32_t oldTile,
        uint32_t newTile,
        std::function<void(int32_t, int32_t, uint32_t)> onTileChanged
    )
		: m_map(std::move(map))  // std::move for efficiency
        , m_x(x)                 
        , m_y(y)
        , m_oldTile(oldTile)
        , m_newTile(newTile)
        , m_onTileChanged(std::move(onTileChanged))
    {
    }

    void TilePaintCommand::Execute() {
		// Execute = Redo
        if (!m_map) return;

		// Set the tile to the new value
        m_map->SetTileSigned(0, m_x, m_y, m_newTile);

		// Notify via callback
        if (m_onTileChanged) {
            m_onTileChanged(m_x, m_y, m_newTile);
        }
    }

    void TilePaintCommand::Undo() {
		// Undo = Revert to old tile
        if (!m_map) return;

		// Set the tile back to the old value
        m_map->SetTileSigned(0, m_x, m_y, m_oldTile);

		// Notify via callback
        if (m_onTileChanged) {
            m_onTileChanged(m_x, m_y, m_oldTile);
        }
    }

    // ========================================================================
    // TileCollisionPaintCommand Implementation
    // ========================================================================

    // Store the tile coordinates, before/after masks, and the callback needed to notify the scene of changes
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

    // Applies the new collision mask to the tile and fires the changed callback so the scene is marked dirty
    void TileCollisionPaintCommand::Execute() {
        if (!m_map) return;
        m_map->SetCollisionMaskSigned(m_x, m_y, m_newMask);
        if (m_onCollisionChanged) {
            m_onCollisionChanged(m_x, m_y, m_newMask); // Notify listeners that collision data changed
        }
    }

    // Restores the tile's previous collision mask and fires the changed callback to keep save state consistent
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

    void UndoSystem::Initialize(ECS::World* world, size_t maxStackSize) {
        m_world = world;
        m_maxStackSize = std::max(size_t(20), maxStackSize);
        LOG_INFO("[UndoSystem] Initialized with max stack size: " << m_maxStackSize);
    }

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

    void UndoSystem::ExecuteCommand(std::unique_ptr<ICommand> command) {
        if (!command) {
            LOG_WARNING("[UndoSystem] Attempted to execute null command");
            return;
        }

        command->Execute();

        if (dynamic_cast<ComponentPropertyCommand*>(command.get()) != nullptr ||
            dynamic_cast<BatchComponentPropertyCommand*>(command.get()) != nullptr) {
            m_propertyEditEmitted = true;
        }

        if (!m_undoStack.empty()) {
            auto* tail = m_undoStack.back().get();
            if (tail && tail->Coalesce(command.get())) {
                m_redoStack.clear();
                TrimUndoStack();
                return;
            }
        }
        m_undoStack.push_back(std::move(command));

        m_redoStack.clear();

        TrimUndoStack();

        LOG_DEBUG("[UndoSystem] Command executed. Undo stack size: " << m_undoStack.size());
    }

    bool UndoSystem::Undo() {
        if (m_undoStack.empty()) {
            LOG_DEBUG("[UndoSystem] Nothing to undo");
            return false;
        }

        // Get the last command
        auto command = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        // Undo it
        command->Undo();

        // Move to redo stack
        m_redoStack.push_back(std::move(command));

        LOG_INFO("[UndoSystem] Undo performed. Undo stack: " << m_undoStack.size()
            << ", Redo stack: " << m_redoStack.size());

        return true;
    }

    bool UndoSystem::Redo() {
        if (m_redoStack.empty()) {
            LOG_DEBUG("[UndoSystem] Nothing to redo");
            return false;
        }

        // Get the last undone command
        auto command = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        // Redo it
        command->Redo();

        // Move back to undo stack
        m_undoStack.push_back(std::move(command));

        LOG_INFO("[UndoSystem] Redo performed. Undo stack: " << m_undoStack.size()
            << ", Redo stack: " << m_redoStack.size());

        return true;
    }

    void UndoSystem::Clear() {
        m_undoStack.clear();
        m_redoStack.clear();
        LOG_INFO("[UndoSystem] Undo/redo history cleared");
    }

    void UndoSystem::TrimUndoStack() {
        while (m_undoStack.size() > m_maxStackSize) {
            m_undoStack.pop_front();
        }
    }

    // ========================================================================
    // Convenience Methods
    // ========================================================================

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

        m_undoStack.push_back(std::move(command));
        m_redoStack.clear();
        TrimUndoStack();
    }

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

    // Creates and pushes a ReparentEntityCommand so hierarchy drag/drops can be undone and redone
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

    bool UndoSystem::CoalesceReorder(EntityId parentId, const std::vector<EntityId>& after) {
        if (m_undoStack.empty()) {
            return false;
        }

        auto* command = dynamic_cast<ReorderEntitiesCommand*>(m_undoStack.back().get());
        if (!command) {
            return false;
        }

        if (!command->UpdateAfter(parentId, after)) {
            return false;
        }

        m_redoStack.clear();
        return true;
    }

    void UndoSystem::BeginPropertyEdit(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath, const nlohmann::json& oldValue) {
        PropertyKey key{ entityId, componentId, propertyPath };
        if (m_activePropertyEdits.find(key) == m_activePropertyEdits.end()) {
            m_activePropertyEdits.emplace(std::move(key), oldValue);
        }
    }

    void UndoSystem::EndPropertyEdit(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath, const nlohmann::json& newValue,
        ComponentPropertyCommand::ApplyFn applyFn) {
        PropertyKey key{ entityId, componentId, propertyPath };
        auto it = m_activePropertyEdits.find(key);
        if (it == m_activePropertyEdits.end()) {
            return;
        }
        const nlohmann::json& oldVal = it->second;
        if (JsonApproxEqual(oldVal, newValue)) {
            m_activePropertyEdits.erase(it);
            return;
        }
        if (!m_world) {
            m_activePropertyEdits.erase(it);
            return;
        }
        auto cmd = std::make_unique<ComponentPropertyCommand>(
            m_world, entityId, componentId, propertyPath, oldVal, newValue, std::move(applyFn)
        );
        ExecuteCommand(std::move(cmd));
        m_activePropertyEdits.erase(it);
    }

    void UndoSystem::BeginBatchPropertyEdit(const std::unordered_set<EntityId>& entities, ECS::ComponentTypeId componentId, const std::string& propertyPath) {
        if (!m_world) return;

        // Resolve component short name from registry so we can locate JSON data in the serialized entity.
        const auto& metaInfo = ECS::ComponentRegistry::Meta(componentId);
        const std::string compShortName = ECS::ComponentRegistry::GetComponentNameFromHash(metaInfo.TypeHash);
        if (compShortName.empty()) {
            return;
        }

        // Fetch current value for each entity to store as the "before" state
        for (EntityId id : entities) {
            ECS::Entity entity = m_world->Resolve(id);
            if (!m_world->IsAlive(entity)) continue;

            nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
            nlohmann::json* componentData = nullptr;

            if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
                for (auto& comp : entityJson["Components"]) {
                    if (!comp.contains("TypeName") || !comp["TypeName"].is_string()) continue;
                    std::string typeName = comp["TypeName"];
                    if (typeName == compShortName || typeName == "ECS::Components::" + compShortName) {
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
                propertyVal = *componentData;
                size_t start = 0;
                size_t end = propertyPath.find('.');
                while (end != std::string::npos) {
                    std::string part = propertyPath.substr(start, end - start);
                    if (propertyVal.is_object() && propertyVal.contains(part)) {
                        propertyVal = propertyVal[part];
                    } else {
                        propertyVal = nlohmann::json();
                        break;
                    }
                    start = end + 1;
                    end = propertyPath.find('.', start);
                }
                if (!propertyVal.is_null()) {
                    std::string lastPart = propertyPath.substr(start);
                    if (propertyVal.is_object() && propertyVal.contains(lastPart)) {
                        propertyVal = propertyVal[lastPart];
                    } else {
                        propertyVal = nlohmann::json();
                    }
                }
            } else {
                propertyVal = nlohmann::json();
            }

            BeginPropertyEdit(id, componentId, propertyPath, propertyVal);
        }
    }

    void UndoSystem::EndBatchPropertyEdit(const std::unordered_set<EntityId>& entities, ECS::ComponentTypeId componentId, const std::string& propertyPath, const nlohmann::json& newValue,
        ComponentPropertyCommand::ApplyFn applyFn) {
        if (!m_world || !applyFn) return;

        std::vector<BatchComponentPropertyCommand::Entry> entries;
        for (EntityId id : entities) {
            PropertyKey key{ id, componentId, propertyPath };
            auto it = m_activePropertyEdits.find(key);
            if (it == m_activePropertyEdits.end()) continue;

            const nlohmann::json& oldVal = it->second;
            if (JsonApproxEqual(oldVal, newValue)) {
                m_activePropertyEdits.erase(it);
                continue;
            }

            entries.push_back({ id, oldVal, newValue });
            m_activePropertyEdits.erase(it);
        }

        if (!entries.empty()) {
            auto cmd = std::make_unique<BatchComponentPropertyCommand>(
                m_world, componentId, propertyPath, std::move(entries), std::move(applyFn)
            );
            ExecuteCommand(std::move(cmd));
        }
    }

    void UndoSystem::RecordPropertyChange(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath,
        const nlohmann::json& oldValue, const nlohmann::json& newValue, ComponentPropertyCommand::ApplyFn applyFn) {
        if (!m_world) return;
        if (JsonApproxEqual(oldValue, newValue)) return;
        auto cmd = std::make_unique<ComponentPropertyCommand>(
            m_world, entityId, componentId, propertyPath, oldValue, newValue, std::move(applyFn)
        );
        ExecuteCommand(std::move(cmd));
    }

    bool UndoSystem::ConsumePropertyEditEmission() {
        bool was = m_propertyEditEmitted;
        m_propertyEditEmitted = false;
        return was;
    }

} // namespace Editor
