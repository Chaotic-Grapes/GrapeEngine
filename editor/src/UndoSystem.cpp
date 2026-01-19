/* Start Header *****************************************************************/
/*!
\file   UndoSystem.cpp
\author Daniel Kay Neo Zuo Feng
\author Samantha Leong Sher Yen
\date   14th January 2026
\brief
Implementation of the UndoSystem and command types used to support
undo/redo operations within the editor. This system records user actions,
applies them via command objects, and restores previous states when
undoing or redoing actions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "UndoSystem.h"
#include "services/Input.h"
#include "core/Logger.h"

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

        if (!m_world->Has<ECS::Components::LocalTransform>(m_entity)) {
            LOG_WARNING("[UndoSystem] Entity has no LocalTransform component");
            return;
        }

        auto& transform = m_world->Get<ECS::Components::LocalTransform>(m_entity);
        transform.Position = m_newPosition;
        transform.Rotation = m_newRotation;
        transform.Scale = m_newScale;

        LOG_DEBUG("[UndoSystem] Applied transform change to entity " << m_entity.Index);
    }

    void TransformChangeCommand::Undo() {
        if (!m_world || !m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot undo transform change - entity invalid");
            return;
        }

        if (!m_world->Has<ECS::Components::LocalTransform>(m_entity)) {
            LOG_WARNING("[UndoSystem] Entity has no LocalTransform component");
            return;
        }

        auto& transform = m_world->Get<ECS::Components::LocalTransform>(m_entity);
        transform.Position = m_oldPosition;
        transform.Rotation = m_oldRotation;
        transform.Scale = m_oldScale;

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
            if (m_world->Has<ECS::Components::LocalTransform>(entity)) {
                m_hasTransform = true;
                m_savedTransform = m_world->Get<ECS::Components::LocalTransform>(entity);
            }

            if (m_world->Has<ECS::Components::Layer>(entity)) {
                m_hasLayer = true;
                m_savedLayer = m_world->Get<ECS::Components::Layer>(entity);
            }

            if (m_world->Has<ECS::Components::Name>(entity)) {
                m_hasName = true;
                m_savedName = m_world->Get<ECS::Components::Name>(entity);
            }

            if (m_world->Has<ECS::Components::Active>(entity)) {
                m_hasActive = true;
                m_savedActive = m_world->Get<ECS::Components::Active>(entity);
            }

            if (m_world->Has<ECS::Components::SpriteRenderer2D>(entity)) {
                m_hasSprite = true;
                m_savedSprite = m_world->Get<ECS::Components::SpriteRenderer2D>(entity);
            }

            if (m_world->Has<ECS::Components::ShapeBox2D>(entity)) {
                m_hasBox = true;
                m_savedBox = m_world->Get<ECS::Components::ShapeBox2D>(entity);
            }

            if (m_world->Has<ECS::Components::ShapeCircle2D>(entity)) {
                m_hasCircle = true;
                m_savedCircle = m_world->Get<ECS::Components::ShapeCircle2D>(entity);
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
            if (m_world->Has<ECS::Components::LocalTransform>(m_entity)) {
                m_world->Get<ECS::Components::LocalTransform>(m_entity) = m_savedTransform;
            }
            else {
                m_world->Add<ECS::Components::LocalTransform>(m_entity, m_savedTransform);
            }
        }

        if (m_hasLayer) {
            if (m_world->Has<ECS::Components::Layer>(m_entity)) {
                m_world->Get<ECS::Components::Layer>(m_entity) = m_savedLayer;
            }
            else {
                m_world->Add<ECS::Components::Layer>(m_entity, m_savedLayer);
            }
        }

        if (m_hasName) {
            if (m_world->Has<ECS::Components::Name>(m_entity)) {
                m_world->Get<ECS::Components::Name>(m_entity) = m_savedName;
            }
            else {
                m_world->Add<ECS::Components::Name>(m_entity, m_savedName);
            }
        }

        if (m_hasActive) {
            if (m_world->Has<ECS::Components::Active>(m_entity)) {
                m_world->Get<ECS::Components::Active>(m_entity) = m_savedActive;
            }
            else {
                m_world->Add<ECS::Components::Active>(m_entity, m_savedActive);
            }
        }

        if (m_hasSprite) {
            if (m_world->Has<ECS::Components::SpriteRenderer2D>(m_entity)) {
                m_world->Get<ECS::Components::SpriteRenderer2D>(m_entity) = m_savedSprite;
            }
            else {
                m_world->Add<ECS::Components::SpriteRenderer2D>(m_entity, m_savedSprite);
            }
        }

        if (m_hasBox) {
            if (m_world->Has<ECS::Components::ShapeBox2D>(m_entity)) {
                m_world->Get<ECS::Components::ShapeBox2D>(m_entity) = m_savedBox;
            }
            else {
                m_world->Add<ECS::Components::ShapeBox2D>(m_entity, m_savedBox);
            }
        }

        if (m_hasCircle) {
            if (m_world->Has<ECS::Components::ShapeCircle2D>(m_entity)) {
                m_world->Get<ECS::Components::ShapeCircle2D>(m_entity) = m_savedCircle;
            }
            else {
                m_world->Add<ECS::Components::ShapeCircle2D>(m_entity, m_savedCircle);
            }
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " restored (redo creation)");
    }

    void CreateEntityCommand::Undo() {
        // Undo, deletes entity
       // if (!m_world || !m_world->IsAlive(m_entity)) {
       // Undo creation = delete/deactivate entity 
       if (!m_world) {

            LOG_WARNING("[UndoSystem] Cannot undo entity creation - world is null");

            return;

        }



        if (!m_world->IsAlive(m_entity)) {
            LOG_WARNING("[UndoSystem] Cannot undo entity creation - entity already deleted");
            return;
        }

        // Actually destroy the entity
        //m_world->Destroy(m_entity);

        // NEW: Soft Delete - deactivate instead of destroying
   // if (m_world && m_world->IsAlive(m_entity)) {
        if (m_world->Has<ECS::Components::Active>(m_entity)) {
            m_world->Get<ECS::Components::Active>(m_entity).Enabled = false;
        } else {
            // If it doesn't have the component, add it as disabled
            m_world->Add<ECS::Components::Active>(m_entity, ECS::Components::Active{ false });
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " deactivated (undo creation)");

   // }

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
            if (m_world->Has<ECS::Components::LocalTransform>(entity)) {
                m_hasTransform = true;
                m_savedTransform = m_world->Get<ECS::Components::LocalTransform>(entity);
            }

              if (m_world->Has<ECS::Components::Layer>(entity)) {

                m_hasLayer = true;

                m_savedLayer = m_world->Get<ECS::Components::Layer>(entity);

            }

            if (m_world->Has<ECS::Components::Name>(entity)) {

                m_hasName = true;

                m_savedName = m_world->Get<ECS::Components::Name>(entity);

            }

            if (m_world->Has<ECS::Components::Active>(entity)) {

                m_hasActive = true;

                m_savedActive = m_world->Get<ECS::Components::Active>(entity);

            }

            if (m_world->Has<ECS::Components::SpriteRenderer2D>(entity)) {
                m_hasSprite = true;
                m_savedSprite = m_world->Get<ECS::Components::SpriteRenderer2D>(entity);
            }

            if (m_world->Has<ECS::Components::ShapeBox2D>(entity)) {
                m_hasBox = true;
                m_savedBox = m_world->Get<ECS::Components::ShapeBox2D>(entity);
            }

            if (m_world->Has<ECS::Components::ShapeCircle2D>(entity)) {
                m_hasCircle = true;
                m_savedCircle = m_world->Get<ECS::Components::ShapeCircle2D>(entity);
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
        // if (!m_world || !m_world->IsAlive(m_entity)) {
        //     LOG_WARNING("[UndoSystem] Cannot undo entity deletion - entity invalid");
          // Undo deletion = restore entity

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
            if (m_world->Has<ECS::Components::LocalTransform>(m_entity)) {
                m_world->Get<ECS::Components::LocalTransform>(m_entity) = m_savedTransform;
            }
            else {
                m_world->Add<ECS::Components::LocalTransform>(m_entity, m_savedTransform);
            }
        }

         if (m_hasLayer) {

            if (m_world->Has<ECS::Components::Layer>(m_entity)) {

                m_world->Get<ECS::Components::Layer>(m_entity) = m_savedLayer;

            }

            else {

                m_world->Add<ECS::Components::Layer>(m_entity, m_savedLayer);

            }

        }

        if (m_hasName) {

            if (m_world->Has<ECS::Components::Name>(m_entity)) {

                m_world->Get<ECS::Components::Name>(m_entity) = m_savedName;

            }

            else {

                m_world->Add<ECS::Components::Name>(m_entity, m_savedName);

            }

        }

        if (m_hasSprite) {
            if (m_world->Has<ECS::Components::SpriteRenderer2D>(m_entity)) {
                m_world->Get<ECS::Components::SpriteRenderer2D>(m_entity) = m_savedSprite;
            }
            else {
                m_world->Add<ECS::Components::SpriteRenderer2D>(m_entity, m_savedSprite);
            }
        }

        if (m_hasBox) {
            if (m_world->Has<ECS::Components::ShapeBox2D>(m_entity)) {
                m_world->Get<ECS::Components::ShapeBox2D>(m_entity) = m_savedBox;
            }
            else {
                m_world->Add<ECS::Components::ShapeBox2D>(m_entity, m_savedBox);
            }
        }

        if (m_hasCircle) {
            if (m_world->Has<ECS::Components::ShapeCircle2D>(m_entity)) {
                m_world->Get<ECS::Components::ShapeCircle2D>(m_entity) = m_savedCircle;
            }
            else {
                m_world->Add<ECS::Components::ShapeCircle2D>(m_entity, m_savedCircle);
            }
        }

        // Reactivate entity
        // if (m_world->Has<ECS::Components::Active>(m_entity)) {
        //     m_world->Get<ECS::Components::Active>(m_entity).Enabled = true;
          // Restore Active component last (this will re-enable the entity)

        if (m_hasActive) {

            if (m_world->Has<ECS::Components::Active>(m_entity)) {

                m_world->Get<ECS::Components::Active>(m_entity) = m_savedActive;

            }

            else {

                m_world->Add<ECS::Components::Active>(m_entity, m_savedActive);

            }
        }

        if (m_onEntityRestored) {
            m_onEntityRestored();
        }

        LOG_DEBUG("[UndoSystem] Entity " << m_entity.Index << " restored (undo deletion)");
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

} // namespace Editor