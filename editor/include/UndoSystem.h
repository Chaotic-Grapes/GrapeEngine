/* Start Header *****************************************************************/
/*!
\file   UndoSystem.h
\author Samantha Leong Sher Yen
\par    s.leong@digipen.edu
\date   21th January 2026
\brief
Declaration of the undo/redo command system used by the editor.

This module implements a command-based undo/redo framework following the
Command Pattern. It supports reversible editor actions such as transform
modifications, entity creation, and entity deletion by encapsulating each
operation into a command object that can be executed, undone, and redone.

The UndoSystem serves as the editor's central history manager, ensuring that
entity edits and scene interactions remain fully reversible and consistent
with ECS state.

Copyright (C) 2026 DigiPen Institute of Technology.
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

// Forward declaration (global namespace)
class TileMap;

namespace Editor {

    // Snapshot of a single entity for undo/redo restore operations.
    struct EntitySnapshot {
        EntityId Id = ECS::Entity::NPOS32;
        EntityId ParentId = ECS::Entity::NPOS32;
        std::vector<ECS::SerializedComponent> Components;
    };

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
        EntityId m_entityId = ECS::Entity::NPOS32;
        std::function<void()> m_onEntityDeleted;
        std::vector<EntitySnapshot> m_snapshots;
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
        EntityId m_entityId = ECS::Entity::NPOS32;
        std::function<void()> m_onEntityRestored;
        std::vector<EntitySnapshot> m_snapshots;
    };

    // ========================================================================
    // Component Snapshot Command (generic component edits/add/remove)
    // ========================================================================

    class EntityComponentsSnapshotCommand : public ICommand {
    public:
        EntityComponentsSnapshotCommand(
            ECS::World* world,
            ECS::Entity entity,
            std::vector<ECS::SerializedComponent> before,
            std::vector<ECS::SerializedComponent> after
        );

        void Execute() override;
        void Undo() override;

    private:
        ECS::World* m_world;
        ECS::Entity m_entity;
        std::vector<ECS::SerializedComponent> m_before;
        std::vector<ECS::SerializedComponent> m_after;
    };

    // ========================================================================
    // Entity Reorder Command
    // ========================================================================

    class ReorderEntitiesCommand : public ICommand {
    public:
        ReorderEntitiesCommand(
            EntityId parentId,
            std::function<void(const std::vector<EntityId>&)> applyOrder,
            std::vector<EntityId> before,
            std::vector<EntityId> after
        );

        void Execute() override;
        void Undo() override;

        bool UpdateAfter(EntityId parentId, const std::vector<EntityId>& after); // Coalesce to new "after".

    private:
        EntityId m_parentId = ECS::Entity::NPOS32; // Parent id that owns the order list.
        std::function<void(const std::vector<EntityId>&)> m_applyOrder; // Apply hook for order updates.
        std::vector<EntityId> m_before; // Original order for undo.
        std::vector<EntityId> m_after; // New order for redo.
    };

    // ========================================================================
    // Tile Paint Command
    // ========================================================================

    class TilePaintCommand : public ICommand {
    public:
		// Paint a single tile with undo/redo support.
        TilePaintCommand(
            std::shared_ptr<TileMap> map,
            int32_t x, int32_t y,
            uint32_t oldTile,
            uint32_t newTile,
            std::function<void(int32_t, int32_t, uint32_t)> onTileChanged
        );

		// Execute the tile paint (redo).
        void Execute() override;

		// Undo the tile paint.
        void Undo() override;

    private:
		std::shared_ptr<TileMap> m_map;  // Target tilemap.
		int32_t m_x;                     // Tile X coordinate.
		int32_t m_y;                     // Tile Y coordinate.
		uint32_t m_oldTile;              // Previous tile ID.
		uint32_t m_newTile;              // New tile ID.
		std::function<void(int32_t, int32_t, uint32_t)> m_onTileChanged;  // Callback after tile change.
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
        bool CoalesceReorder(EntityId parentId, const std::vector<EntityId>& after); // Merge reorder into last command.

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
