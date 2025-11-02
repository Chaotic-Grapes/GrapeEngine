/*****************************************************************************/
/*!
\file       GameObjectManagement.cpp
\author     Samantha Leong (s.leong@digipen.edu)
\par        DigiPen login: 2403088
\date       2025-11-03
\brief
Implements the GameObjectEditor class responsible for managing game objects
within the in-editor environment. This includes level loading/saving,
object creation, deletion, cloning, property editing, and interactive
in-world picking and dragging. It integrates ImGui-based editor windows
for hierarchy management, property editing, and file operations.

This file acts as the main bridge between the engine ECS and the editor UI.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without prior
written consent of DigiPen Institute of Technology is prohibited.
*/
/*****************************************************************************/


#include "../editor/GameObjectManagement.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "services/DebugUI.h"
#include "services/Input.h"
#include <imgui.h>
//#include "services/UICommon.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "serialization/EntitySerializer.h"
#include "core/Profiler.h"
#include "helpers/MathHelper.h"
#include <filesystem>
#include "core/Logger.h"
#include "audio/FmodAudioDevice.h"
#include "audio/SoundTypes.h"

 // --- Level Management Implementation ---

 // Assumed directory for level files, please change as needed
static constexpr const char* LEVEL_DIR = "assets/levels/";

/**
 * @brief Saves the current level to a file.
 *
 * @param filename Full path and filename to save the level data to.
 * @exception std::exception If serialization or file I/O fails.
 */
void GameObjectEditor::SaveLevel(const std::string& filename) {
	if (!HasValidWorld()) return;
	try {
		// Assumed: EntitySerializer::SerializeWorld exists and handles file output
		Serialization::EntitySerializer::SerializeWorld(*m_world, filename);
		LOG_INFO("Successfully saved level to: " << filename);
	}
	catch (const std::exception& e) {
		LOG_ERROR("Failed to save world: " << e.what());
	}
}

/**
 * @brief Loads a level from a JSON file and replaces the current world.
 *
 * @param filename Full path and filename to load the level data from.
 * @exception std::exception If deserialization or file reading fails.
 */
void GameObjectEditor::LoadLevel(const std::string& filename) {
	if (!HasValidWorld()) return;
	try {
		// 1. Clear current level before loading a new one
		ClearAllGameObjects();

		// 2. Assumed: EntitySerializer::DeserializeWorld exists and handles entity creation
		Serialization::EntitySerializer::DeserializeWorld(*m_world, filename);
		LOG_INFO("Successfully loaded level from: " << filename);

		m_selectedEntityId = 0; // Deselect everything on load
		_invalidateCache();
	}
	catch (const std::exception& e) {
		LOG_ERROR("Failed to load world: " << e.what());
	}
}

// --- In-World Picking/Dragging Implementation ---

/**
 * @brief Handles mouse-based in-world interactions such as selecting and dragging game objects.
 *
 * Uses circle collider picking and updates object positions during drag operations.
 */
void GameObjectEditor::HandleInWorldInteraction() {
	if (!HasValidWorld()) return;

	// State for dragging
	static bool isDragging = false;

	// Only proceed if the mouse is not interacting with an ImGui window
	if (ImGui::GetIO().WantCaptureMouse) return;

	// Get mouse position
	double xPos, yPos;
	Input::GetMousePosition(xPos, yPos);
	Vector2D mouseWorldPos(static_cast<float>(xPos), static_cast<float>(yPos));

	// --- 1. Picking (Select/Start Drag) ---
	if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
		EntityId newSelection = 0;

		// Iterate over objects in reverse order to pick the visually top-most object.
		const auto entities = m_world->GetEntityManager().GetAllEntities();
		for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
			auto entity = m_world->GetEntityManager().GetEntity(*it);
			auto* transform = entity.GetComponent<Component::Transform>();
			auto* collider = entity.GetComponent<Component::CircleCollider2D>(); // Assuming circle collider for picking

			// Only pick if it has a Transform and a Collider/Sizing component
			if (transform && collider) {
				// Simple circle-based pick: Check if mouse is inside the object's radius
				float distance = MathHelper::Distance(transform->Position, mouseWorldPos); // Assumed helper exists
				if (distance <= collider->Radius) {
					newSelection = *it;
					isDragging = true;
					break; // Found the top-most object
				}
			}
		}

		// Update selection state
		if (m_selectedEntityId != newSelection) {
			m_selectedEntityId = newSelection;
			_invalidateCache();
		}
	}

	// --- 2. Dragging ---
	if (isDragging && m_selectedEntityId != 0 && Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
		auto selectedEntity = m_world->GetEntityManager().GetEntity(m_selectedEntityId);
		if (selectedEntity.GetId() != 0) {
			// Directly set position to mouse world position
			selectedEntity.Transform().Position = mouseWorldPos;
		}
	}

	// --- 3. Drop (Stop Dragging) ---
	if (isDragging && !Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
		isDragging = false;
	}
}


// --- Editor Windows Entry Point ---

/**
 * @brief Displays all editor windows, including the main menu, hierarchy, and property editor.
 */
void GameObjectEditor::ShowEditorWindows() {
	_showMainMenu();
	_showHierarchyWindow();
	_showPropertyEditorWindow();
}


// --- 1. Main Menu (for Save/Load) ---

/**
 * @brief Renders the editor's main menu bar, including file operations for saving and loading levels.
 *
 * Displays ImGui modals for save/load dialogs and allows users to select existing JSON files from disk.
 */
void GameObjectEditor::_showMainMenu() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			// Save Level
			if (ImGui::MenuItem("Save Level...")) {
				ImGui::OpenPopup("Save Level");
			}

			// Load Level
			if (ImGui::MenuItem("Load Level...")) {
				ImGui::OpenPopup("Load Level");
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "Alt+F4")) {
				// Handle application exit
			}

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// Modal for Save Level
	if (ImGui::BeginPopupModal("Save Level", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char filenameBuffer[128] = "NewLevel";
		ImGui::Text("Enter level filename (will be saved in %s):", LEVEL_DIR);
		ImGui::InputText("##savefilename", filenameBuffer, sizeof(filenameBuffer));

		if (ImGui::Button("Save") && strlen(filenameBuffer) > 0) {
			std::string fullPath = std::string(LEVEL_DIR) + std::string(filenameBuffer) + ".json";
			SaveLevel(fullPath);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Modal for Load Level
	if (ImGui::BeginPopupModal("Load Level", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		static std::string selectedFile = "";
		ImGui::Text("Select level to load from %s:", LEVEL_DIR);

		if (ImGui::BeginListBox("##LevelList", ImVec2(200, 150))) {
			try {
				for (const auto& entry : std::filesystem::directory_iterator(LEVEL_DIR)) {
					if (entry.path().extension() == ".json") {
						std::string filename = entry.path().filename().string();
						bool is_selected = (selectedFile == filename);
						if (ImGui::Selectable(filename.c_str(), is_selected)) {
							selectedFile = filename;
						}
					}
				}
			}
			catch (const std::exception& e) {
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error reading dir: %s", e.what());
			}
			ImGui::EndListBox();
		}

		if (ImGui::Button("Load") && !selectedFile.empty()) {
			LoadLevel(std::string(LEVEL_DIR) + selectedFile);
			selectedFile = ""; // Reset selection
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			selectedFile = "";
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}


// --- 2. Hierarchy Objects List Window ---

/**
 * @brief Renders the hierarchy window showing all active game objects in the current world.
 *
 * Allows object selection, addition, cloning, and deletion via context menus.
 */
void GameObjectEditor::_showHierarchyWindow() {
	// Use config values (Assumed for window docking/position)
	// UICommon::ApplyLayout(UICommon::WindowId::DEBUG_EDITOR); 

	ImGui::Begin("Hierarchy Objects List");

	// Add new game object section (kept here for quick access)
	ImGui::Text("Create New Object:");
	static char nameBuffer[DebugUIConfig::MAX_OBJECT_NAME_LENGTH] = "NewObject";
	ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
	ImGui::InputText("##NewObjectName", nameBuffer, sizeof(nameBuffer));

	ImGui::SameLine();
	if (ImGui::Button("Add Object") && strlen(nameBuffer) > 0) {
		AddGameObject(nameBuffer);
	}

	ImGui::Separator();

	// Display list of current objects
	const auto entities = m_world->GetEntityManager().GetAllEntities();
	ImGui::Text("Current Objects (%zu):", entities.size());

	// Scrollable region for the list
	ImGui::BeginChild("ObjectsScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);

	// For each object
	for (const auto& entId : entities) {
		auto entity = m_world->GetEntityManager().GetEntity(entId);
		if (entity.GetId() == 0) continue;

		std::stringstream oss;
		oss << "[" << entity.GetId() << "] " << entity.GetName();
		std::string label = oss.str();

		// CORE HIERARCHY / SELECTION LOGIC
		const bool is_selected = (m_selectedEntityId == entId);

		// Use Selectable for the hierarchy list item
		if (ImGui::Selectable(label.c_str(), is_selected)) {
			// Select the object, or deselect if already selected
			m_selectedEntityId = is_selected ? 0 : entId;
		}

		// Handle right-click context menu on the selected item
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::Selectable("Clone")) {
				CloneGameObject(entity);
			}
			if (ImGui::Selectable("Delete")) {
				RemoveGameObject(entId);
				if (m_selectedEntityId == entId) {
					m_selectedEntityId = 0; // Deselect if deleted
				}
				ImGui::EndPopup();
				break; // Break the loop after deletion
			}
			ImGui::EndPopup();
		}
	}

	ImGui::EndChild();

	// Clear all button
	ImGui::Separator();
	if (ImGui::Button("Clear All Objects")) {
		ClearAllGameObjects();
		m_selectedEntityId = 0; // Deselect everything
	}

	ImGui::End();
}


// --- 3. Level and Content Editor - Property Editor Window ---

/**
 * @brief Renders the property editor window that allows modifying selected entity components.
 *
 * Supported components include Transform, ShapeRenderer2D, and CircleCollider2D.
 */
void GameObjectEditor::_showPropertyEditorWindow() {
	// Use config values
	// UICommon::ApplyLayout(UICommon::WindowId::PROPERTY_EDITOR); 

	ImGui::Begin("Property Editor");

	// Check if an entity is selected
	if (m_selectedEntityId != 0 && HasValidWorld()) {
		auto selectedEntity = m_world->GetEntityManager().GetEntity(m_selectedEntityId);

		if (selectedEntity.GetId() == 0) {
			m_selectedEntityId = 0;
			ImGui::Text("Selected Entity is no longer valid.");
			ImGui::End();
			return;
		}

		// Entity Header and Deselect button
		ImGui::Text("Selected Object: %s (ID: %u)", selectedEntity.GetName().c_str(), m_selectedEntityId);
		ImGui::SameLine(ImGui::GetWindowWidth() - 70);
		if (ImGui::SmallButton("Deselect")) {
			m_selectedEntityId = 0;
			ImGui::End();
			return;
		}
		ImGui::Separator();

		ImGui::BeginChild("PropertyScroll", ImVec2(0, 0), false);

		// ** Component Property Editing: Transform Component **
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto* transform = selectedEntity.GetComponent<Component::Transform>();
			if (transform) {
				float inputWidth = (ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x * 3) / 2.0f;

				// Position
				ImGui::Text("Position");
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat(std::string("X##Pos" + std::to_string(m_selectedEntityId)).c_str(), &transform->Position.X, 0.0f, 0.0f, "%.2f");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat(std::string("Y##Pos" + std::to_string(m_selectedEntityId)).c_str(), &transform->Position.Y, 0.0f, 0.0f, "%.2f");

				// Rotation
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat(std::string("Rotation##Rot" + std::to_string(m_selectedEntityId)).c_str(), &transform->Rotation, 0.0f, 0.0f, "%.2f");

				// Scale
				ImGui::Text("Scale");
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat(std::string("X##Scale" + std::to_string(m_selectedEntityId)).c_str(), &transform->Scale.X, 0.0f, 0.0f, "%.2f");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat(std::string("Y##Scale" + std::to_string(m_selectedEntityId)).c_str(), &transform->Scale.Y, 0.0f, 0.0f, "%.2f");
			}
		}

		// ** Component Property Editing: ShapeRenderer2D Component **
		if (ImGui::CollapsingHeader("ShapeRenderer2D")) {
			auto* shapeRenderer = selectedEntity.GetComponent<Component::ShapeRenderer2D>();
			if (shapeRenderer) {
				ImGui::ColorEdit4(std::string("Fill Color##Color" + std::to_string(m_selectedEntityId)).c_str(), (float*) & shapeRenderer->FillColor.R);

				// Radius (Specific to Circle type)
				if (shapeRenderer->Type == Component::ShapeRenderer2D::ShapeType::Circle) {
					ImGui::SetNextItemWidth(100.f);
					ImGui::InputFloat(std::string("Radius##Rad" + std::to_string(m_selectedEntityId)).c_str(), &shapeRenderer->Radius, 1.0f, 10.0f, "%.2f");
				}

				// Future: Add logic for other shape types (Square, Sprite, etc.)
			}
		}

		// ** Component Property Editing: CircleCollider2D Component **
		if (ImGui::CollapsingHeader("CircleCollider2D")) {
			auto* collider = selectedEntity.GetComponent<Component::CircleCollider2D>();
			if (collider) {
				ImGui::SetNextItemWidth(100.f);
				ImGui::InputFloat(std::string("Radius##ColRad" + std::to_string(m_selectedEntityId)).c_str(), &collider->Radius, 1.0f, 10.0f, "%.2f");
			}
		}

		ImGui::EndChild();

	}
	else {
		ImGui::Text("No Game Object Selected.");
	}

	ImGui::End();
}

/**
 * @brief Checks if the editor currently holds a valid world reference.
 *
 * @return true If a valid world is available.
 * @return false Otherwise.
 */
// Check if World object exists
bool GameObjectEditor::HasValidWorld() const {
    return m_world != nullptr;
}


/**
 * @brief Creates and registers a new game object within the editor and ECS world.
 *
 * @param name The desired name of the new game object.
 */
// Create a new object
void GameObjectEditor::AddGameObject(const std::string& name) {
    // Safety check
    if (name.empty() || name.length() > m_config.MAX_OBJECT_NAME_LENGTH
        || !HasValidWorld()) return;

    // Create real ECS entity with components
    auto entity = _createGameEntity(name);

    entity.Transform().Position.X = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowWidth()));
    entity.Transform().Position.Y = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowHeight()));

    // Add to editor list for UI display + clear cached toggle/delete
    // button labels so new objects get proper unique labels
    _invalidateCache();
}

/**
 * @brief Removes a game object from the world using its entity ID.
 *
 * @param id The ID of the entity to remove.
 */
// Find and delete an object by ID
void GameObjectEditor::RemoveGameObject(const EntityId id) {
    // Safety check
    if (!HasValidWorld()) return;

    const auto entity = m_world->GetEntityManager().GetEntity(id);
    m_world->GetEntityManager().DestroyEntity(entity);

    _invalidateCache();
}

/**
 * @brief Creates a clone of the given game object, offset slightly in position.
 *
 * @param entity The entity to be cloned.
 */
void GameObjectEditor::CloneGameObject(const Entity& entity) {
    // Safety check
    if (!HasValidWorld()) return;

    auto cloned = entity.Clone();

    // Offset position so clone doesn't overlap original
    auto& transform = cloned.Transform();
    transform.Position.X += 50.0f;
    transform.Position.Y += 50.0f;

    _invalidateCache();
}

/**
 * @brief Removes all game objects from the current world.
 */
void GameObjectEditor::ClearAllGameObjects() {
    // Safety check
    if (!HasValidWorld()) return;

    m_world->GetEntityManager().DestroyAllEntities();

    _invalidateCache();
}

//void GameObjectEditor::_showGameObjectEditor() {
//    // Use config values
//    UICommon::ApplyLayout(UICommon::WindowId::DEBUG_EDITOR);
//
//    ImGui::Begin("Game Object Editor");
//
//    // Add new game object section
//    ImGui::Text("Create New Object:");
//    static char nameBuffer[DebugUIConfig::MAX_OBJECT_NAME_LENGTH];
//    if (m_newObjectName.length() < sizeof(nameBuffer)) {
//        // Store new object name
//        strcpy_s(nameBuffer, m_newObjectName.c_str());
//    }
//
//    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
//        m_newObjectName = nameBuffer;
//    }
//
//    // When button is clicked, create the object
//    if (ImGui::Button("Add Object") && !m_newObjectName.empty()) {
//        AddGameObject(m_newObjectName);
//        m_newObjectName = "NewObject"; // Reset
//    }
//
//    ImGui::Separator();
//
//    // Quick add buttons for common objects
//    ImGui::Text("Quick Add:");
//    if (ImGui::Button("Player")) AddGameObject("Player");
//    ImGui::SameLine();  // Make the next button appear on the same line instead of below
//    if (ImGui::Button("Enemy")) AddGameObject("Enemy");
//    ImGui::SameLine();
//    if (ImGui::Button("Collectible")) AddGameObject("Collectible");
//
//    ImGui::Separator();
//
//    static char prefabName[128] = "sample-enemy-prefab";
//    ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));
//    if (ImGui::Button("Load Prefab") && strlen(prefabName) > 0) {
//        std::ifstream file("assets/samples/" + std::string(prefabName) + ".prefab");
//        if (!file.is_open()) {
//            LOG_ERROR("Cannot open file: " << prefabName);
//        }
//        else {
//            try {
//                auto entityJson = nlohmann::json::parse(file);
//                file.close();
//
//                // Deserialize creates the entity internally
//                (void)Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);
//
//                _invalidateCache();
//            }
//            catch (const std::exception& e) {
//                LOG_ERROR("Failed to parse prefab file: " << e.what());
//            }
//        }
//    }
//
//    // Display list of current objects
//    const auto entities = m_world->GetEntityManager().GetAllEntities();
//    ImGui::Text("Current Objects (%zu):", entities.size());
//
//    // For each object
//    for (const auto& entId : entities) {
//        auto entity = m_world->GetEntityManager().GetEntity(entId); // Ensure entity is valid
//
//        // Active status
//        std::stringstream oss;
//        oss << "[" << entity.GetId() << "] " << entity.GetName();
//        if (ImGui::CollapsingHeader(oss.str().c_str(), _getCollapsedHeaderBool(entId))) {
//            // Delete button for each object
//            if (ImGui::SmallButton(_getDeleteLabel(entId).c_str())) {
//                RemoveGameObject(entId);
//                break;
//            }
//
//            // Clone button for each object
//            ImGui::SameLine();
//            if (ImGui::SmallButton(_getCloneLabel(entId).c_str())) {
//                CloneGameObject(entity);
//                break;
//            }
//
//            ImGui::SeparatorText("Transform");
//
//            // Get pointer to transform component to ensure we're modifying the actual component
//            auto* transform = entity.GetComponent<Component::Transform>();
//            if (transform) {
//                // BEFORE modification
//                ImGui::Text("DEBUG: Current Scale: (%.2f, %.2f)", transform->Scale.X, transform->Scale.Y);
//
//                ImGui::Text("Position");
//                ImGui::SetNextItemWidth(100.f);
//                ImGui::InputFloat(std::string("X##P" + std::to_string(entId)).c_str(), &transform->Position.X);
//                ImGui::SameLine();
//                ImGui::SetNextItemWidth(100.f);
//                ImGui::InputFloat(std::string("Y##P" + std::to_string(entId)).c_str(), &transform->Position.Y);
//
//                ImGui::SetNextItemWidth(100.f);
//                ImGui::InputFloat(std::string("Rotation##" + std::to_string(entId)).c_str(), &transform->Rotation);
//
//                ImGui::Text("Scale");
//                ImGui::SetNextItemWidth(100.f);
//                if (ImGui::InputFloat(std::string("X##S" + std::to_string(entId)).c_str(), &transform->Scale.X)) {
//                    // Print when value changes
//                    std::cout << "Scale.X changed to: " << transform->Scale.X << std::endl;
//                }
//                ImGui::SameLine();
//                ImGui::SetNextItemWidth(100.f);
//                if (ImGui::InputFloat(std::string("Y##S" + std::to_string(entId)).c_str(), &transform->Scale.Y)) {
//                    std::cout << "Scale.Y changed to: " << transform->Scale.Y << std::endl;
//                }
//            }
//        }
//    }
//    ImGui::Separator();
//
//    // Clear all buttons
//    if (ImGui::Button("Clear All Objects")) {
//        ClearAllGameObjects();
//    }
//
//    ImGui::End();
//}

/**
 * @brief Helper function that creates a new ECS entity with default components.
 *
 * Adds Transform, ShapeRenderer2D, and CircleCollider2D components by default.
 *
 * @param name The name of the new entity.
 * @return The created Entity.
 */
// Helper function to create entities with basic components
Entity GameObjectEditor::_createGameEntity(const std::string& name) {
    // Create new entity in ECS
    auto entity = m_world->CreateEntity(name);

    // Add basic components that most game objects need
    entity.AddComponent<Component::Transform>();

    // Visual components
    auto& shapeRenderer = entity.AddComponent<Component::ShapeRenderer2D>();
    shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
    shapeRenderer.Radius = 35.0f;

    // Set color based on type
    if (name == "Player") shapeRenderer.FillColor = Color(0.0f, 0.0f, 1.0f, 1.0f);
    else if (name == "Enemy") shapeRenderer.FillColor = Color(1.0f, 0.0f, 0.0f, 1.0f);
    else if (name == "Collectible") shapeRenderer.FillColor = Color(1.0f, 1.0f, 0.0f, 1.0f);
    else shapeRenderer.FillColor = Color(1.0f, 1.0f, 1.0f, 1.0f);

    // Add CircleCollider2D so physics test can detect and add physics
    entity.AddComponent<Component::CircleCollider2D>(35.0f);

    return entity;
}

/**
 * @brief Clears cached UI labels for delete/clone buttons to ensure unique naming.
 */
// Clear cached button labels
void GameObjectEditor::_invalidateCache() {
    m_cachedDeleteLabels.clear();
    m_cachedCloneLabels.clear();
}

/**
 * @brief Retrieves or generates a unique delete button label for a given entity ID.
 *
 * @param id Entity ID.
 * @return const std::string& Reference to the generated label.
 */
const std::string& GameObjectEditor::_getDeleteLabel(const EntityId id) const {
    // Same thing
    auto it = m_cachedDeleteLabels.find(id);
    if (it == m_cachedDeleteLabels.end()) {
        // Build label
        std::string label = "Delete##" + std::to_string(id);
        it = m_cachedDeleteLabels.insert({ id, label }).first;
    }
    return it->second;
}

/**
 * @brief Retrieves or generates a unique clone button label for a given entity ID.
 *
 * @param id Entity ID.
 * @return const std::string& Reference to the generated label.
 */
const std::string& GameObjectEditor::_getCloneLabel(const EntityId id) const {
    // Same thing
    auto it = m_cachedCloneLabels.find(id);
    if (it == m_cachedCloneLabels.end()) {
        // Build label
        std::string label = "Clone##" + std::to_string(id);
        it = m_cachedCloneLabels.insert({ id, label }).first;
    }
    return it->second;
}

/**
 * @brief Retrieves the collapsed header state for an entity in the hierarchy list.
 *
 * @param id Entity ID.
 * @return const bool& Reference to the collapse state.
 */
const bool& GameObjectEditor::_getCollapsedHeaderBool(const EntityId id) const {
    auto it = m_cachedCollapsedHeaders.find(id);
    if (it == m_cachedCollapsedHeaders.end()) {
        it = m_cachedCollapsedHeaders.insert({ id, false }).first;
    }
    return it->second;
}
