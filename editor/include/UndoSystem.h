/* Start Header *****************************************************************/
/*!
\file    UndoSystem.h
\author  Samantha Leong Sher Yen (70%)
         Foo Rui Qin (30%)
\par     s.leong@digipen.edu
         ruiqin.foo@digipen.edu
\date    11th March 2026
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
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>

// Forward declaration (global namespace)
class TileMap;

namespace Editor {

    // Snapshot of a single entity used for undo/redo restore operations
    struct EntitySnapshot {
        EntityId Id = ECS::Entity::NPOS32;                  // Entity ID being snapshotted
        EntityId ParentId = ECS::Entity::NPOS32;            // Parent entity ID at snapshot time
        std::vector<ECS::SerializedComponent> Components;   // Full component data at snapshot time
    };

    // ========================================================================
    // Command Interface (Command Pattern)
    // ========================================================================

    class ICommand {
    public:
        virtual ~ICommand() = default;

        // Execute the command (apply the action)
        virtual void Execute() = 0;

        // Undo the command (reverse the action)
        virtual void Undo() = 0;

        // Redo the command (defaults to re-executing)
        virtual void Redo() { Execute(); }

        // Attempt to merge this command with a newer one of the same type
        // Returns true if coalesced successfully, false otherwise
        virtual bool Coalesce(ICommand* other) { (void)other; return false; }
    };

    // ========================================================================
    // Macro Command (group multiple commands into one)
    // ========================================================================

    // Groups multiple commands into a single undoable unit
    class MacroCommand : public ICommand {
    public:
        MacroCommand() = default;

        // Append a command to the end of the macro group
        void AddCommand(std::unique_ptr<ICommand> command) { m_commands.push_back(std::move(command)); }

        // Execute all commands in forward order
        void Execute() override { for (auto& cmd : m_commands) cmd->Execute(); }

        // Undo all commands in reverse order
        void Undo() override { for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) (*it)->Undo(); }

        // Redo all commands in forward order
        void Redo() override { for (auto& cmd : m_commands) cmd->Redo(); }

        // Return true if the macro contains no commands
        bool IsEmpty() const { return m_commands.empty(); }

    private:
        std::vector<std::unique_ptr<ICommand>> m_commands;  // Ordered list of grouped commands
    };

    // ========================================================================
    // Transform Change Command
    // ========================================================================

    // Records and reverses a change to an entity's local transform
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

        // Apply the new transform values to the entity
        void Execute() override;

        // Restore the entity's transform to its state before the change
        void Undo() override;

    private:
        ECS::World* m_world;        // ECS world containing the entity
        ECS::Entity m_entity;       // Entity whose transform is being modified

        Vector3D m_oldPosition;     // Position before the change
        Quaternion m_oldRotation;   // Rotation before the change
        Vector3D m_oldScale;        // Scale before the change

        Vector3D m_newPosition;     // Position after the change
        Quaternion m_newRotation;   // Rotation after the change
        Vector3D m_newScale;        // Scale after the change
    };

    // ========================================================================
    // Entity Creation Command
    // ========================================================================

    // Records and reverses an entity creation action
    class CreateEntityCommand : public ICommand {
    public:
        CreateEntityCommand(
            ECS::World* world,
            ECS::Entity entity,
            std::function<void()> onEntityDeleted = nullptr
        );

        // Mark the created entity as active in the world
        void Execute() override;

        // Delete the entity, reversing the creation
        void Undo() override;

    private:
        ECS::World* m_world;                            // ECS world containing the entity
        EntityId m_entityId = ECS::Entity::NPOS32;      // ID of the created entity
        std::function<void()> m_onEntityDeleted;        // Optional callback fired when entity is deleted on undo
        std::vector<EntitySnapshot> m_snapshots;        // Snapshots of the entity and its descendants
    };

    // ========================================================================
    // Entity Deletion Command
    // ========================================================================

    // Records and reverses an entity deletion action, storing entity state for restoration
    class DeleteEntityCommand : public ICommand {
    public:
        DeleteEntityCommand(
            ECS::World* world,
            ECS::Entity entity,
            std::function<void()> onEntityRestored = nullptr
        );

        // Delete the entity from the world
        void Execute() override;

        // Restore the entity and its components from the stored snapshot
        void Undo() override;

    private:
        ECS::World* m_world;                            // ECS world containing the entity
        EntityId m_entityId = ECS::Entity::NPOS32;      // ID of the deleted entity
        std::function<void()> m_onEntityRestored;       // Optional callback fired when entity is restored on undo
        std::vector<EntitySnapshot> m_snapshots;        // Snapshots of the entity and its descendants
    };

    // ========================================================================
    // Component Snapshot Command (generic component edits/add/remove)
    // ========================================================================

    // Records and reverses a bulk component state change on an entity
    class EntityComponentsSnapshotCommand : public ICommand {
    public:
        EntityComponentsSnapshotCommand(
            ECS::World* world,
            ECS::Entity entity,
            std::vector<ECS::SerializedComponent> before,
            std::vector<ECS::SerializedComponent> after
        );

        // Apply the after-state component data to the entity
        void Execute() override;

        // Restore the before-state component data to the entity
        void Undo() override;

    private:
        ECS::World* m_world;                                    // ECS world containing the entity
        EntityId m_entityId = ECS::Entity::NPOS32;              // ID of the affected entity
        std::vector<ECS::SerializedComponent> m_before;         // Component state before the change
        std::vector<ECS::SerializedComponent> m_after;          // Component state after the change
    };

    // ========================================================================
    // Entity Reorder Command
    // ========================================================================

    // Records and reverses a reorder of entities under a common parent
    class ReorderEntitiesCommand : public ICommand {
    public:
        ReorderEntitiesCommand(
            EntityId parentId,
            std::function<void(const std::vector<EntityId>&)> applyOrder,
            std::vector<EntityId> before,
            std::vector<EntityId> after
        );

        // Apply the new child order under the parent entity
        void Execute() override;

        // Restore the original child order under the parent entity
        void Undo() override;

        // Coalesce a subsequent reorder into this command by updating the after-state
        // Returns true if the parent matches and the update was applied
        bool UpdateAfter(EntityId parentId, const std::vector<EntityId>& after);

    private:
        EntityId m_parentId = ECS::Entity::NPOS32;                          // Parent entity that owns the order list
        std::function<void(const std::vector<EntityId>&)> m_applyOrder;     // Callback to apply a new order
        std::vector<EntityId> m_before;                                     // Original entity order for undo
        std::vector<EntityId> m_after;                                      // New entity order for redo
    };

    // ========================================================================
    // Entity Reparent Command
    // ========================================================================

    // Records and reverses moving an entity to a different parent in the hierarchy
    class ReparentEntityCommand : public ICommand {
    public:
        ReparentEntityCommand(
            ECS::World* world,
            EntityId childId,
            EntityId oldParentId,
            EntityId newParentId
        );

        // Detach the entity from its old parent and attach it under the new parent
        void Execute() override;

        // Detach the entity from the new parent and reattach it under the old parent
        void Undo() override;

    private:
        // Shared logic used by both Execute and Undo to apply a parent assignment
        // parentId = NPOS32 detaches the child to root
        void ApplyParent(EntityId parentId);

        ECS::World* m_world = nullptr;
        EntityId m_childId = ECS::Entity::NPOS32;       // Entity being reparented
        EntityId m_oldParentId = ECS::Entity::NPOS32;   // Previous parent used for undo
        EntityId m_newParentId = ECS::Entity::NPOS32;   // Target parent used for execute
    };

    // ========================================================================
    // Component Property Command
    // ========================================================================

    // Records and reverses a single property change on a component
    class ComponentPropertyCommand : public ICommand {
    public:
        using ApplyFn = std::function<void(ECS::World*, ECS::Entity, ECS::ComponentTypeId, const std::string&, const nlohmann::json&)>;

        ComponentPropertyCommand(
            ECS::World* world,
            EntityId entityId,
            ECS::ComponentTypeId componentId,
            std::string propertyPath,
            nlohmann::json oldValue,
            nlohmann::json newValue,
            ApplyFn applyFn
        );

        // Apply the new property value to the live component
        void Execute() override;

        // Restore the old property value to the live component
        void Undo() override;

        // Merge a newer edit on the same property into this command; returns true if merged
        bool Coalesce(ICommand* other) override;

    private:
        ECS::World* m_world = nullptr;
        EntityId m_entityId = ECS::Entity::NPOS32;      // Entity whose component is being edited
        ECS::ComponentTypeId m_componentId = 0;         // Type ID of the affected component
        std::string m_propertyPath;                     // JSON path to the property being changed
        nlohmann::json m_oldValue;                      // Property value before the change
        nlohmann::json m_newValue;                      // Property value after the change
        ApplyFn m_applyFn;                              // Callback to apply a value to the live component
    };

    // ========================================================================
    // Batch Component Property Command
    // ========================================================================

    // Records and reverses a property change applied to multiple entities at once
    class BatchComponentPropertyCommand : public ICommand {
    public:
        using ApplyFn = ComponentPropertyCommand::ApplyFn;

        // Per-entity old/new value pair for the batch edit
        struct Entry {
            EntityId Entity = ECS::Entity::NPOS32;  // Entity being edited
            nlohmann::json OldValue;                // Property value before the change
            nlohmann::json NewValue;                // Property value after the change
        };

        BatchComponentPropertyCommand(
            ECS::World* world,
            ECS::ComponentTypeId componentId,
            std::string propertyPath,
            std::vector<Entry> entries,
            ApplyFn applyFn
        );

        // Apply the new property value to all entities in the batch
        void Execute() override;

        // Restore the old property value to all entities in the batch
        void Undo() override;

        // Merge a newer batch edit on the same property into this command; returns true if merged
        bool Coalesce(ICommand* other) override;

    private:
        ECS::World* m_world = nullptr;
        ECS::ComponentTypeId m_componentId = 0;     // Type ID of the affected component
        std::string m_propertyPath;                 // JSON path to the property being changed
        std::vector<Entry> m_entries;               // Per-entity old/new value pairs
        ApplyFn m_applyFn;                          // Callback to apply a value to a live component
    };

    // ========================================================================
    // Tile Paint Command
    // ========================================================================

    // Records and reverses a single tile paint or erase on a tilemap
    class TilePaintCommand : public ICommand {
    public:
        TilePaintCommand(
            std::shared_ptr<TileMap> map,
            int32_t x, int32_t y,
            uint32_t oldTile,
            uint32_t newTile,
            std::function<void(int32_t, int32_t, uint32_t)> onTileChanged
        );

        // Write the new tile ID at the target cell (redo)
        void Execute() override;

        // Restore the old tile ID at the target cell
        void Undo() override;

    private:
        std::shared_ptr<TileMap> m_map;                                         // Target tilemap
        int32_t m_x;                                                            // Tile X coordinate
        int32_t m_y;                                                            // Tile Y coordinate
        uint32_t m_oldTile;                                                     // Tile ID before the paint
        uint32_t m_newTile;                                                     // Tile ID after the paint
        std::function<void(int32_t, int32_t, uint32_t)> m_onTileChanged;        // Callback fired after each tile change
    };

    // ========================================================================
    // Tile Collision Paint Command
    // ========================================================================

    // Records and reverses a single collision mask paint on a tilemap
    class TileCollisionPaintCommand : public ICommand {
    public:
        TileCollisionPaintCommand(
            std::shared_ptr<TileMap> map,
            int32_t x,
            int32_t y,
            uint8_t oldMask,
            uint8_t newMask,
            std::function<void(int32_t, int32_t, uint8_t)> onCollisionChanged
        );

        // Write the new collision mask at the target cell (redo)
        void Execute() override;

        // Restore the old collision mask at the target cell
        void Undo() override;

    private:
        std::shared_ptr<TileMap> m_map;                                               // Target tilemap
        int32_t m_x;                                                                  // Tile X coordinate
        int32_t m_y;                                                                  // Tile Y coordinate
        uint8_t m_oldMask;                                                            // Collision mask before the paint
        uint8_t m_newMask;                                                            // Collision mask after the paint
        std::function<void(int32_t, int32_t, uint8_t)> m_onCollisionChanged;          // Callback fired after each collision change
    };

    // ========================================================================
    // Undo System Manager
    // ========================================================================

    // Central undo/redo history manager for all reversible editor actions
    class UndoSystem {
    public:
        // -------------------------------------------------------------------------
        // Lifecycle
        // -------------------------------------------------------------------------

        // Initialize the undo system with the given world and maximum stack size
        void Initialize(ECS::World* world, size_t maxStackSize = 50);

        // Process keyboard input for undo/redo (Ctrl+Z / Ctrl+Y) � call every frame
        void Update();

        // -------------------------------------------------------------------------
        // Core Operations
        // -------------------------------------------------------------------------

        // Execute a command and push it onto the undo stack
        void ExecuteCommand(std::unique_ptr<ICommand> command);

        // Undo the last command; returns true if successful, false if stack is empty
        bool Undo();

        // Redo the last undone command; returns true if successful, false if nothing to redo
        bool Redo();

        // Clear all undo and redo history
        void Clear();

        // -------------------------------------------------------------------------
        // State Query
        // -------------------------------------------------------------------------

        // Return true if there are commands available to undo
        bool CanUndo() const { return !m_undoStack.empty(); }

        // Return true if there are commands available to redo
        bool CanRedo() const { return !m_redoStack.empty(); }

        // Return the number of available undo steps
        size_t GetUndoCount() const { return m_undoStack.size(); }

        // Return the number of available redo steps
        size_t GetRedoCount() const { return m_redoStack.size(); }

        // -------------------------------------------------------------------------
        // Convenience Recording Methods
        // -------------------------------------------------------------------------

        // Record and execute a transform change for an entity
        void RecordTransformChange(
            EntityId entityId,
            const Vector3D& oldPos, const Quaternion& oldRot, const Vector3D& oldScale,
            const Vector3D& newPos, const Quaternion& newRot, const Vector3D& newScale
        );

        // Record and execute an entity creation action
        void RecordEntityCreation(EntityId entityId);

        // Record and execute an entity deletion action
        void RecordEntityDeletion(EntityId entityId);

        // Record and execute an entity reparent action
        void RecordEntityReparent(EntityId childId, EntityId oldParentId, EntityId newParentId);

        // Attempt to merge a reorder into the last command on the stack
        // Returns true if the coalesce was successful
        bool CoalesceReorder(EntityId parentId, const std::vector<EntityId>& after);

        // -------------------------------------------------------------------------
        // Property Edit API
        // -------------------------------------------------------------------------

        // Begin tracking a property edit, storing the old value for undo
        void BeginPropertyEdit(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath, 
            const nlohmann::json& oldValue);

        // Finalize a property edit, storing the new value and pushing the command
        void EndPropertyEdit(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath, 
            const nlohmann::json& newValue, ComponentPropertyCommand::ApplyFn applyFn);

        // Begin tracking a batch property edit across multiple entities
        void BeginBatchPropertyEdit(const std::unordered_set<EntityId>& entities, ECS::ComponentTypeId componentId, 
            const std::string& propertyPath);

        // Finalize a batch property edit and push the command
        void EndBatchPropertyEdit(const std::unordered_set<EntityId>& entities, ECS::ComponentTypeId componentId, 
            const std::string& propertyPath, const nlohmann::json& newValue, ComponentPropertyCommand::ApplyFn applyFn);

        // Record an immediate single-step property change without begin/end tracking
        void RecordPropertyChange(EntityId entityId, ECS::ComponentTypeId componentId, const std::string& propertyPath,
            const nlohmann::json& oldValue, const nlohmann::json& newValue, ComponentPropertyCommand::ApplyFn applyFn);

        // Return true and reset the emission flag if a property edit was emitted this frame
        // Used to avoid double-undo with the snapshot system
        bool ConsumePropertyEditEmission();

    private:
        // -------------------------------------------------------------------------
        // Internal Helpers
        // -------------------------------------------------------------------------

        // Trim the undo stack to the maximum size, discarding the oldest entries
        void TrimUndoStack();

        // -------------------------------------------------------------------------
        // State
        // -------------------------------------------------------------------------

        ECS::World* m_world = nullptr;                              // ECS world for entity operations
        size_t m_maxStackSize = 50;                                 // Maximum number of undo steps retained

        std::deque<std::unique_ptr<ICommand>> m_undoStack;          // Stack of executed commands (oldest at front)
        std::deque<std::unique_ptr<ICommand>> m_redoStack;          // Stack of undone commands (most recent at front)

        bool m_propertyEditEmitted = false;                         // Whether a property edit command was pushed this frame

        // Composite key identifying a specific property on a specific entity's component
        struct PropertyKey {
            EntityId Entity;
            ECS::ComponentTypeId Component;
            std::string Property;
            bool operator==(const PropertyKey& other) const {
                return Entity == other.Entity && Component == other.Component && Property == other.Property;
            }
        };

        // Hash functor for PropertyKey used in unordered_map
        struct PropertyKeyHasher {
            /**
             * @brief Compute hash for PropertyKey combining entity, component, and property name.
             * @param k Composite property key.
             * @return Hash value suitable for unordered_map bucketing.
             */
            size_t operator()(const PropertyKey& k) const {
                size_t h1 = std::hash<uint32_t>{}(k.Entity);
                size_t h2 = std::hash<uint32_t>{}(k.Component);
                size_t h3 = std::hash<std::string>{}(k.Property);
                return ((h1 * 1315423911u) ^ (h2 << 5)) ^ h3;
            }
        };

        // In-progress property edits keyed by entity/component/property, storing old values
        std::unordered_map<PropertyKey, nlohmann::json, PropertyKeyHasher> m_activePropertyEdits;
    };

} // namespace Editor

#endif // UNDO_SYSTEM_H