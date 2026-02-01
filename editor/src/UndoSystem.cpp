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

namespace Editor {

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
        , m_entity(entity)
        , m_onEntityDeleted(onEntityDeleted)
    {
        // Snapshot entity state at creation time
        if (m_world && m_world->IsAlive(entity)) {
            auto* transform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
                m_world, entity, "LocalTransform");
            if (transform) {
                m_hasTransform = true;
                m_savedTransform = *transform;
            }

            auto* layer = Editor::ECSUtils::GetComponentPtr<ECS::Components::Layer>(
                m_world, entity, "Layer");
            if (layer) {
                m_hasLayer = true;
                m_savedLayer = *layer;
            }

            auto* name = Editor::ECSUtils::GetComponentPtr<ECS::Components::Name>(
                m_world, entity, "Name");
            if (name) {
                m_hasName = true;
                m_savedName = *name;
            }

            auto* active = Editor::ECSUtils::GetComponentPtr<ECS::Components::Active>(
                m_world, entity, "Active");
            if (active) {
                m_hasActive = true;
                m_savedActive = *active;
            }

            auto* sprite = Editor::ECSUtils::GetComponentPtr<ECS::Components::SpriteRenderer2D>(
                m_world, entity, "SpriteRenderer2D");
            if (sprite) {
                m_hasSprite = true;
                m_savedSprite = *sprite;
            }

            auto* box = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeBox2D>(
                m_world, entity, "ShapeBox2D");
            if (box) {
                m_hasBox = true;
                m_savedBox = *box;
            }

            auto* circle = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeCircle2D>(
                m_world, entity, "ShapeCircle2D");
            if (circle) {
                m_hasCircle = true;
                m_savedCircle = *circle;
            }
        }
    }

    void CreateEntityCommand::Execute() {
        // Execute = Restore the entity mainly for Redo
        if (!m_world || !m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot execute entity creation - entity invalid");
            return;
        }

        // Restore all components from snapshot
        if (m_hasTransform) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "LocalTransform", m_savedTransform);
        }

        if (m_hasLayer) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "Layer", m_savedLayer);
        }

        if (m_hasName) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "Name", m_savedName);
        }

        if (m_hasActive) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "Active", m_savedActive);
        }

        if (m_hasSprite) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "SpriteRenderer2D", m_savedSprite);
        }

        if (m_hasBox) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "ShapeBox2D", m_savedBox);
        }

        if (m_hasCircle) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "ShapeCircle2D", m_savedCircle);
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " restored (redo creation)");
    }

    void CreateEntityCommand::Undo() {
        // Undo creation = soft delete (deactivate)
        if (!m_world) {
            LOG_WARNING("[UndoSystem] Cannot undo entity creation - world is null");
            return;
        }

        if (!m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot undo entity creation - entity already deleted");
            return;
        }

        if (m_world->Has<ECS::Components::Active>(m_entity)) {
            m_world->Get<ECS::Components::Active>(m_entity).Enabled = false;
        }
        else {
            m_world->Add<ECS::Components::Active>(m_entity, ECS::Components::Active{ false });
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " deactivated (undo creation)");

        if (m_onEntityDeleted) {
            m_onEntityDeleted();
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " deleted (undo creation)");
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
        , m_entity(entity)
        , m_onEntityRestored(onEntityRestored)
    {
        // Snapshot entity state before deletion
        if (m_world && m_world->IsAlive(entity)) {
            auto* transform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
                m_world, entity, "LocalTransform");
            if (transform) {
                m_hasTransform = true;
                m_savedTransform = *transform;
            }

            auto* layer = Editor::ECSUtils::GetComponentPtr<ECS::Components::Layer>(
                m_world, entity, "Layer");
            if (layer) {
                m_hasLayer = true;
                m_savedLayer = *layer;
            }

            auto* name = Editor::ECSUtils::GetComponentPtr<ECS::Components::Name>(
                m_world, entity, "Name");
            if (name) {
                m_hasName = true;
                m_savedName = *name;
            }

            auto* active = Editor::ECSUtils::GetComponentPtr<ECS::Components::Active>(
                m_world, entity, "Active");
            if (active) {
                m_hasActive = true;
                m_savedActive = *active;
            }

            auto* sprite = Editor::ECSUtils::GetComponentPtr<ECS::Components::SpriteRenderer2D>(
                m_world, entity, "SpriteRenderer2D");
            if (sprite) {
                m_hasSprite = true;
                m_savedSprite = *sprite;
            }

            auto* box = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeBox2D>(
                m_world, entity, "ShapeBox2D");
            if (box) {
                m_hasBox = true;
                m_savedBox = *box;
            }

            auto* circle = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeCircle2D>(
                m_world, entity, "ShapeCircle2D");
            if (circle) {
                m_hasCircle = true;
                m_savedCircle = *circle;
            }
        }
    }

    void DeleteEntityCommand::Execute() {
        if (!m_world || !m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot execute entity deletion - entity invalid");
            return;
        }

        // Deactivate entity
        if (m_world->Has<ECS::Components::Active>(m_entity)) {
            m_world->Get<ECS::Components::Active>(m_entity).Enabled = false;
        }
        else {
            m_world->Add<ECS::Components::Active>(m_entity, ECS::Components::Active{ false });
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " deleted");
    }

    void DeleteEntityCommand::Undo() {
        if (!m_world) {
            LOG_WARNING("[UndoSystem] Cannot undo entity deletion - world is null");
            return;
        }

        // For soft delete system: entity should still be alive, just deactivated
        if (!m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot undo entity deletion - entity no longer exists in ECS");
            return;
        }

        // Restore entity state
        if (m_hasTransform) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "LocalTransform", m_savedTransform);
        }

        if (m_hasLayer) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "Layer", m_savedLayer);
        }

        if (m_hasName) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "Name", m_savedName);
        }

        if (m_hasSprite) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "SpriteRenderer2D", m_savedSprite);
        }

        if (m_hasBox) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "ShapeBox2D", m_savedBox);
        }

        if (m_hasCircle) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "ShapeCircle2D", m_savedCircle);
        }

        // Restore Active component last (this will re-enable the entity)
        if (m_hasActive) {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "Active", m_savedActive);
        }
        else {
            Editor::ECSUtils::SetComponent(m_world, m_entity, "Active", ECS::Components::Active{ true });
        }

        if (m_onEntityRestored) {
            m_onEntityRestored();
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " restored (undo deletion)");
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
    // UndoSystem Implementation
    // ========================================================================

    void UndoSystem::Initialize(ECS::World* world, size_t maxStackSize) {
        m_world = world;
        m_maxStackSize = std::max(size_t(20), maxStackSize);
        LOG_INFO("[UndoSystem] Initialized with max stack size: " << m_maxStackSize);
    }

    void UndoSystem::Update() {
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

        // Execute the command
        command->Execute();

        // Add to undo stack
        m_undoStack.push_back(std::move(command));

        // Clear redo stack
        m_redoStack.clear();

        // Trim undo stack if it exceeds maximum size
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

} // namespace Editor
