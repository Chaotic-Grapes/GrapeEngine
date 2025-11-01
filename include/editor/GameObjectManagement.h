/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file GameObjectManagement.h
 * @brief 
 */


#ifndef GAMEOBJECTEDITOR_H
#define GAMEOBJECTEDITOR_H


#include <vector>
#include <string>
#include <unordered_map>
#include "ecs/Entity.h"



class GameObjectEditor {
public:

	/**
 * @brief Set the world reference for entity management
 * @param world Pointer to the World object
 *
 * Updates the world reference used for creating and managing entities
 * through the debug interface.
 */
	void SetWorld(World* world) { m_world = world; }


	/**
	* @brief Check if a valid world reference exists
	* @return bool True if world pointer is valid, false otherwise
	*/
	bool HasValidWorld() const;


	/**
	 * @brief Add a new game object to the world
	 * @param name Name for the new game object
	 *
	 * Creates a new entity with basic components and adds it to the world.
	 * The object is positioned randomly within the window bounds.
	 */
	void AddGameObject(const std::string& name);

	/**
	 * @brief Remove a game object by entity ID
	 * @param id Entity ID of the object to remove
	 *
	 * Finds and destroys the entity with the specified ID, removing it
	 * from the world and updating the UI cache.
	 */
	void RemoveGameObject(EntityId id);

	/**
	 * @brief Clone an existing game object
	 * @param entity Entity to clone
	 *
	 * Creates a copy of the specified entity with slightly offset position
	 * to avoid overlapping with the original.
	 */
	void CloneGameObject(const Entity& entity);

	/**
	 * @brief Clear all game objects from the world
	 *
	 * Destroys all entities in the world and updates the UI cache.
	 * Use with caution as this removes all game objects.
	 */
	void ClearAllGameObjects();

	/**
	 * @brief Render the game object editor window
	 *
	 * Provides interface for creating, editing, cloning, and deleting
	 * game objects with real-time entity management.
	 */
	//void _showGameObjectEditor();

	/**
	 * @brief Renders all editor windows (Hierarchy, Property Editor, Main Menu).
	 *
	 * Replaces the original _showGameObjectEditor call in the main loop.
	 */
	void ShowEditorWindows();

	/**
	 * @brief Handles selection and dragging of objects using mouse input.
	 *
	 * Should be called in the main editor update loop BEFORE rendering.
	 */
	void HandleInWorldInteraction();

	/**
	 * @brief Saves the current world/level state to a JSON file.
	 * @param filename Full path and name of the file to save to.
	 */
	void SaveLevel(const std::string& filename);

	/**
	 * @brief Loads a new world/level state from a JSON file.
	 * @param filename Full path and name of the file to load from.
	 */
	void LoadLevel(const std::string& filename);

	/**
	 * @brief Sets the currently selected entity.
	 */
	void SetSelectedEntityId(EntityId id) { m_selectedEntityId = id; }

private:
	DebugUIConfig m_config;     ///< Configuration settings for UI layout and appearance
	World* m_world;             ///< Pointer to World object for entity management
	bool m_enabled = false;     ///< Flag indicating if debug UI is currently enabled
	bool m_initialized = false; ///< Flag indicating if ImGui has been initialized

	// UI state
	bool m_showDemo = false;                    ///< Flag to show/hide ImGui demo window
	std::string m_newObjectName = "NewObject"; ///< Default name for new game objects

	// Event counters for input debugging
	int m_spacePressed = 0;   ///< Counter for space key press events
	int m_spaceReleased = 0;  ///< Counter for space key release events

	// State for the currently selected object
	EntityId m_selectedEntityId = 0;

	// Helper functions to draw specific editor windows
	void _showMainMenu();
	void _showHierarchyWindow();
	void _showPropertyEditorWindow();

	// Cached UI elements to avoid string creation every frame
	mutable std::unordered_map<EntityId, std::string> m_cachedDeleteLabels;    ///< Cached delete button labels
	mutable std::unordered_map<EntityId, std::string> m_cachedCloneLabels;     ///< Cached clone button labels
	mutable std::unordered_map<EntityId, bool> m_cachedCollapsedHeaders;       ///< Cached header collapse states

	/**
	* @brief Create a new game entity with basic components
	* @param name Name for the new entity
	* @return Entity The created entity with default components
	*
	* Helper method that creates an entity with transform and other
	* basic components needed for game objects.
	*/
	Entity _createGameEntity(const std::string& name);

	/**
	 * @brief Invalidate UI caches when entities change
	 *
	 * Clears cached button labels and states when entities are added,
	 * removed, or modified to ensure UI consistency.
	 */
	void _invalidateCache();

	/**
 * @brief Get cached delete button label for entity
 * @param id Entity ID
 * @return const std::string& Cached delete button label
 */
	const std::string& _getDeleteLabel(EntityId id) const;

	/**
	 * @brief Get cached clone button label for entity
	 * @param id Entity ID
	 * @return const std::string& Cached clone button label
	 */
	const std::string& _getCloneLabel(EntityId id) const;

	/**
	 * @brief Get cached header collapse state for entity
	 * @param id Entity ID
	 * @return const bool& Cached header collapse state
	 */
	const bool& _getCollapsedHeaderBool(EntityId id) const;
};

#endif