/* Start Header *****************************************************************/
/*!
\file   EditorEntityActions.h
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   16th November 2025

\brief
Provides editor-side tools for creating, deleting, cloning and reparenting
entities in the active scene.

Used by panels to perform scene operations without touching low level ECS logic
Stores the current scene so actions stay consistent across scene loads.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_ENTITY_ACTIONS_H
#define EDITOR_ENTITY_ACTIONS_H

#include <string>
#include "Scene/Scene.h"
#include "Scene/SceneManager.h"
#include "ecs/Entity.h"
#include "EditorFileMenu.h"
#include "UndoSystem.h"

// Provides editor-side operations for creating, deleting, cloning and reparenting 
// entities in the active scene
class EntityActions {
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    // Creates the action helper and binds it to the initial scene
    EntityActions(Scenes::Scene* scene);

    // Updates which scene the editor is currently editing
    void SetScene(Scenes::Scene* scene);

    // -------------------------------------------------------------------------
    // Entity Creation and Destruction
    // -------------------------------------------------------------------------

    // Creates a new entity in the active scene with a Name and Transform
    EntityId AddEntity(const std::string& name, EntityId parent = ECS::Entity::NPOS32);

    // Deletes an entity and all its children from the scene
    // Returns true if the entity existed and deletion was performed
    bool RemoveEntity(EntityId id);

    // Removes every entity in the scene
    void ClearAllEntities();

    // -------------------------------------------------------------------------
    // Entity Hierarchy Operations
    // -------------------------------------------------------------------------

    // Creates a full copy of an entity including all components and children
    EntityId CloneEntity(EntityId id);

    // Moves an entity under a new parent in the hierarchy
    void ReparentEntity(EntityId child, EntityId newParent);


    // Set file menu reference for dirty tracking
    void SetFileMenu(EditorFileMenu* fileMenu) { m_fileMenu = fileMenu; }

    // Undo System
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

private:
    // The currently edited scene (world + hierarchy)
    Scenes::Scene* m_scene;
    EditorFileMenu* m_fileMenu = nullptr;

    // Undo System
    Editor::UndoSystem* m_undoSystem = nullptr;
};

#endif
