/* Start Header *****************************************************************/
/*!
\file   UndoSystem.h
\author Daniel Kay Neo Zuo Feng
\author Samantha Leong Sher Yen
\date   14th January 2026
\brief
Declaration of the undo/redo command system used by the editor.

This module implements a command-based undo/redo framework following the
Command Pattern. It supports reversible editor actions such as transform
modifications, entity creation, and entity deletion by encapsulating each
operation into a command object that can be executed, undone, and redone.

The UndoSystem serves as the editor's central history manager, ensuring that
entity edits and scene interactions remain fully reversible and consistent
with ECS state.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef UNDO_SYSTEM_H
#define UNDO_SYSTEM_H

#include "ecs/World.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include "Math/Vector3D.h"
#include "Math/Quaternion.h"
#include <memory>
#include <vector>
#include <deque>
#include <functional>

namespace Editor {

    // ========================================================================
    // Command Interface (Command Pattern)
    // ========================================================================

    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual void Redo() { Execute(); }
    };

    // ========================================================================
    // Transform Change Command
    // ========================================================================

    class TransformChangeCommand : public ICommand {
    public:
        TransformChangeCommand(
            ECS::World* world,
            ECS::Entity entity,
            const Vector3D& oldPos,
            const Quaternion& oldRot,
            const Vector3D& oldScale,
            const Vector3D& newPos,
            const Quaternion& newRot,
            const Vector3D& newScale
        );

        void Execute() override;
        void Undo() override;

    private:
        ECS::World* m_world;
        ECS::Entity m_entity;

        // Old state
        Vector3D m_oldPosition;
        Quaternion m_oldRotation;
        Vector3D m_oldScale;

        // New state
        Vector3D m_newPosition;
        Quaternion m_newRotation;
        Vector3D m_newScale;
    };

    // ========================================================================
    // Entity Creation Command
    // ========================================================================

    class CreateEntityCommand : public ICommand {
    public:
        CreateEntityCommand(
            ECS::World* world,
            ECS::Entity entity,
            std::function<void()> onEntityDeleted = nullptr
        );

        void Execute() override;    // Mark entity as active
        void Undo() override;       // Delete entity

    private:
        ECS::World* m_world;
        ECS::Entity m_entity;
        std::function<void()> m_onEntityDeleted;
        bool m_wasExecuted = false;

        // Store entity states for restoration
        bool m_hasTransform = false;
        ECS::Components::LocalTransform m_savedTransform;

        bool m_hasLayer = false;
        ECS::Components::Layer m_savedLayer;

        bool m_hasName = false;
        ECS::Components::Name m_savedName;
        
        bool m_hasActive = false;
        ECS::Components::Active m_savedActive;

        bool m_hasLayer = false;
        ECS::Components::Layer m_savedLayer;

        bool m_hasName = false;
        ECS::Components::Name m_savedName;

        bool m_hasActive = false;
        ECS::Components::Active m_savedActive;

        bool m_hasSprite = false;
        ECS::Components::SpriteRenderer2D m_savedSprite;

        bool m_hasBox = false;
        ECS::Components::ShapeBox2D m_savedBox;

        bool m_hasCircle = false;
        ECS::Components::ShapeCircle2D m_savedCircle;
    };

    // ========================================================================
    // Entity Deletion Command
    // ========================================================================

    /*!
    \class DeleteEntityCommand
    \brief Handles undo/redo for entity deletion.
    Stores entity state for restoration.
    */
    class DeleteEntityCommand : public ICommand {
    public:
        DeleteEntityCommand(
            ECS::World* world,
            ECS::Entity entity,
            std::function<void()> onEntityRestored = nullptr
        );

        void Execute() override;   // Delete entity
        void Undo() override;      // Restore entity

    private:
        ECS::World* m_world;
        ECS::Entity m_entity;
        std::function<void()> m_onEntityRestored;

        // Store entity state
        bool m_hasTransform = false;
        ECS::Components::LocalTransform m_savedTransform;

        bool m_hasLayer = false;
        ECS::Components::Layer m_savedLayer;

        bool m_hasName = false;
        ECS::Components::Name m_savedName;

        bool m_hasActive = false;
        ECS::Components::Active m_savedActive;
        
        bool m_hasSprite = false;
        ECS::Components::SpriteRenderer2D m_savedSprite;

        bool m_hasBox = false;
        ECS::Components::ShapeBox2D m_savedBox;

        bool m_hasCircle = false;
        ECS::Components::ShapeCircle2D m_savedCircle;
    };

    // ========================================================================
    // Undo System Manager
    // ========================================================================

    class UndoSystem {
    public:
        /*!
        \brief Initialize the undo system.
        \param world The ECS world to operate on.
        \param maxStackSize Maximum number of undo steps to a minimum of 20.
        */
        void Initialize(ECS::World* world, size_t maxStackSize = 50);

        /*!
        \brief Process keyboard input for undo/redo.
        Call this every frame to check for CTRL+Z / CTRL+Y.
        */
        void Update();

        /*!
        \brief Execute a command and add it to the undo stack.
        \param command The command to execute.
        */
        void ExecuteCommand(std::unique_ptr<ICommand> command);

        /*!
        \brief Undo the last command.
        \return True if undo was successful, false if stack is empty.
        */
        bool Undo();

        /*!
        \brief Redo the last undone command.
        \return True if redo was successful, false if nothing to redo.
        */
        bool Redo();

        /*!
        \brief Clear all undo/redo history.
        */
        void Clear();

        /*!
        \brief Check if undo is available.
        */
        bool CanUndo() const { return !m_undoStack.empty(); }

        /*!
        \brief Check if redo is available.
        */
        bool CanRedo() const { return !m_redoStack.empty(); }

        /*!
        \brief Get the number of available undo steps.
        */
        size_t GetUndoCount() const { return m_undoStack.size(); }

        /*!
        \brief Get the number of available redo steps.
        */
        size_t GetRedoCount() const { return m_redoStack.size(); }

        // Convenience methods for common operations
        void RecordTransformChange(
            EntityId entityId,
            const Vector3D& oldPos, const Quaternion& oldRot, const Vector3D& oldScale,
            const Vector3D& newPos, const Quaternion& newRot, const Vector3D& newScale
        );

        void RecordEntityCreation(EntityId entityId);
        void RecordEntityDeletion(EntityId entityId);

    private:
        ECS::World* m_world = nullptr;
        size_t m_maxStackSize = 50;

        std::deque<std::unique_ptr<ICommand>> m_undoStack;
        std::deque<std::unique_ptr<ICommand>> m_redoStack;

        // Maintain redo stack when clearing old undo entries
        void TrimUndoStack();
    };

} // namespace Editor

#endif // UNDO_SYSTEM_H