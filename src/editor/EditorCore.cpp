/* Start Header *****************************************************************/
/*!
\file   EditorCore.cpp
\author Samantha Leong (80%)
		Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
		ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements the EditorCore class for core editor functionality and entity management.
- EditorCore = Entity/Level management (Model/Controller layer)
- Centralized entity operations: add, remove, clone, clear, reparent
- Handles in-world entity picking/dragging in viewport
- Future: Level save/load management
*/
/* End Header *******************************************************************/

#include "../editor/EditorCore.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "services/Input.h"
#include <imgui.h>
#include "helpers/MathHelper.h"
#include <filesystem>
#include "core/Logger.h"

// Directory for level files (used by commented out save/load)
static constexpr const char* LEVEL_DIR = "assets/levels/";

// Set up fonts and world reference
void EditorCore::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world) {
	m_mainFont = mainFont;
	m_boldFont = boldFont;
	m_symbolsFont = symbolsFont;
	m_world = world;
}

// LEVEL MANAGEMENT (commented out for now, kept for future use)
// 
// Save current level to JSON file
// void EditorCore::SaveLevel(const std::string& filename) {
// 	if (!HasValidWorld()) return;
// 	try {
// 		Serialization::EntitySerializer::SerializeWorld(*m_world, filename);
// 		LOG_INFO("Successfully saved level to: " << filename);
// 	}
// 	catch (const std::exception& e) {
// 		LOG_ERROR("Failed to save world: " << e.what());
// 	}
// }
//
// Load level from JSON file and replace current world
// void EditorCore::LoadLevel(const std::string& filename) {
// 	if (!HasValidWorld()) return;
// 	try {
// 		ClearAllGameObjects();
// 		Serialization::EntitySerializer::DeserializeWorld(*m_world, filename);
// 		LOG_INFO("Successfully loaded level from: " << filename);
// 		m_selectedEntityId = 0;
// 		_invalidateCache();
// 	}
// 	catch (const std::exception& e) {
// 		LOG_ERROR("Failed to load world: " << e.what());
// 	}
// }

// Handle mouse based entity selection and dragging in viewport
// Uses circle collider radius for picking hitbox
void EditorCore::HandleInWorldInteraction() {
	if (!HasValidWorld()) return;

	// Track if we are currently dragging an entity
	static bool isDragging = false;

	// Don't interfere if mouse is over ImGui windows
	if (ImGui::GetIO().WantCaptureMouse) return;

	// Get current mouse position in world space
	double xPos, yPos;
	Input::GetMousePosition(xPos, yPos);
	Vector2D mouseWorldPos(static_cast<float>(xPos), static_cast<float>(yPos));

	// PICKING: Start selecting/dragging on left click
	if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
		EntityId newSelection = 0;

		// Iterate entities in reverse to pick topmost rendered object first
		const auto entities = m_world->GetEntityManager().GetAllEntities();
		for (auto it = entities.rbegin(); it != entities.rend(); it++) {
			// // Fetch entity by ID, get transform & collider
			auto entity = m_world->GetEntityManager().GetEntity(*it);
			auto* transform = entity.GetComponent<Component::Transform>();
			auto* collider = entity.GetComponent<Component::CircleCollider2D>();

			// Need both transform and collider for picking
			if (transform && collider) {
				// Simple circle based picking: is mouse inside radius
				float distance = MathHelper::Distance(transform->Position, mouseWorldPos);
				if (distance <= collider->Radius) {
					newSelection = *it;
					isDragging = true;
					break; // Found topmost object
				}
			}
		}

		// Update selection if changed
		if (m_selectedEntityId != newSelection) {
			m_selectedEntityId = newSelection;
			_invalidateCache();
		}
	}

	// DRAGGING: Move entity to follow mouse while button held
	if (isDragging && m_selectedEntityId != 0 && Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
		auto selectedEntity = m_world->GetEntityManager().GetEntity(m_selectedEntityId);
		if (selectedEntity.GetId() != 0) {
			// Directly set position to mouse world position
			selectedEntity.Transform().Position = mouseWorldPos;
		}
	}

	// DROP: Stop dragging when button released
	if (isDragging && !Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
		isDragging = false;
	}
}

// Render main menu bar (currently just File menu)
void EditorCore::ShowEditorWindows() {
	_showMainMenu();
}

// Render File menu with save/load options (currently disabled)
void EditorCore::_showMainMenu() {
	// Black background for menu bar
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
	// Remove border from menu bar only
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			// Save Level (disabled for now)
			if (ImGui::MenuItem("Save Level")) {
				ImGui::OpenPopup("Save Level");
			}

			// Load Level (disabled for now)
			if (ImGui::MenuItem("Load Level")) {
				ImGui::OpenPopup("Load Level");
			}

			ImGui::Separator();

			// Exit option
			if (ImGui::MenuItem("Exit", "Alt+F4")) {
				// TODO: Handle application exit
			}

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	// SAVE/LOAD MODALS (commented out, kept for future use)
	//
	// Modal popup for saving level with filename input
	// if (ImGui::BeginPopupModal("Save Level", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
	// 	static char filenameBuffer[128] = "NewLevel";
	// 	ImGui::Text("Enter level filename (will be saved in %s):", LEVEL_DIR);
	// 	ImGui::InputText("##savefilename", filenameBuffer, sizeof(filenameBuffer));
	//
	// 	if (ImGui::Button("Save") && strlen(filenameBuffer) > 0) {
	// 		std::string fullPath = std::string(LEVEL_DIR) + std::string(filenameBuffer) + ".json";
	// 		SaveLevel(fullPath);
	// 		ImGui::CloseCurrentPopup();
	// 	}
	// 	ImGui::SameLine();
	// 	if (ImGui::Button("Cancel")) {
	// 		ImGui::CloseCurrentPopup();
	// 	}
	// 	ImGui::EndPopup();
	// }

	// Modal popup for loading level with file list selection
	// if (ImGui::BeginPopupModal("Load Level", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
	// 	static std::string selectedFile = "";
	// 	ImGui::Text("Select level to load from %s:", LEVEL_DIR);
	//
	// 	if (ImGui::BeginListBox("##LevelList", ImVec2(200, 150))) {
	// 		try {
	// 			for (const auto& entry : std::filesystem::directory_iterator(LEVEL_DIR)) {
	// 				if (entry.path().extension() == ".json") {
	// 					std::string filename = entry.path().filename().string();
	// 					bool is_selected = (selectedFile == filename);
	// 					if (ImGui::Selectable(filename.c_str(), is_selected)) {
	// 						selectedFile = filename;
	// 					}
	// 				}
	// 			}
	// 		}
	// 		catch (const std::exception& e) {
	// 			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error reading dir: %s", e.what());
	// 		}
	// 		ImGui::EndListBox();
	// 	}
	//
	// 	if (ImGui::Button("Load") && !selectedFile.empty()) {
	// 		LoadLevel(std::string(LEVEL_DIR) + selectedFile);
	// 		selectedFile = "";
	// 		ImGui::CloseCurrentPopup();
	// 	}
	// 	ImGui::SameLine();
	// 	if (ImGui::Button("Cancel")) {
	// 		selectedFile = "";
	// 		ImGui::CloseCurrentPopup();
	// 	}
	// 	ImGui::EndPopup();
	// }
}

// Check if world pointer is valid before doing operations
bool EditorCore::HasValidWorld() const {
	return m_world != nullptr;
}

// Create new entity with given name at random screen position
void EditorCore::AddEntity(const std::string& name, EntityId parentId) {
	// Validate name and world
	if (name.empty() || name.length() > MAX_OBJECT_NAME_LENGTH || !HasValidWorld())
		return;

	// Create entity with default components
	auto entity = _createGameEntity(name);

	// Set parent if specified
	if (parentId != 0) {
		auto* transform = entity.GetComponent<Component::Transform>();
		if (transform) transform->ParentId = parentId;
	}

	// Random position so entities don't spawn on top of each other
	entity.Transform().Position.X = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowWidth()));
	entity.Transform().Position.Y = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowHeight()));

	// Clear cached UI labels for new entity
	_invalidateCache();
}

// Delete entity by ID from world (optionally recursive for children)
void EditorCore::RemoveEntity(const EntityId id, bool recursive) {
	if (!HasValidWorld()) return;

	// If recursive, delete all children first
	if (recursive) {
		auto children = _getChildren(id);
		for (auto childId : children) {
			RemoveEntity(childId, true); // Recursively delete children
		}
	}

	const auto entity = m_world->GetEntityManager().GetEntity(id);
	m_world->GetEntityManager().DestroyEntity(entity);

	_invalidateCache();
}

// Clone entity with slight position offset so it doesn't overlap original
void EditorCore::CloneEntity(const EntityId id) {
	if (!HasValidWorld()) return;

	auto entity = m_world->GetEntityManager().GetEntity(id);
	if (entity.GetId() == 0) return; // Invalid entity

	auto cloned = entity.Clone();

	// Offset so clone is visible next to original
	auto& transform = cloned.Transform();
	transform.Position.X += 50.0f;
	transform.Position.Y += 50.0f;

	_invalidateCache();
}

// Delete all entities in world
void EditorCore::ClearAllEntities() {
	if (!HasValidWorld()) return;
	m_world->GetEntityManager().DestroyAllEntities();
	_invalidateCache();
}

// Create entity with default components: Transform, ShapeRenderer2D, CircleCollider2D
// Color is set based on entity name for quick visual identification
Entity EditorCore::_createGameEntity(const std::string& name) {
	auto entity = m_world->CreateEntity(name);

	// Transform is required for all entities
	entity.AddComponent<Component::Transform>();

	// Visual shape for rendering
	auto& shapeRenderer = entity.AddComponent<Component::ShapeRenderer2D>();
	shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
	shapeRenderer.Radius = 35.0f;

	// Color based on name for easy identification
	if (name == "Player")
		shapeRenderer.FillColor = Color(0.0f, 0.0f, 1.0f, 1.0f); // Blue
	else if (name == "Enemy")
		shapeRenderer.FillColor = Color(1.0f, 0.0f, 0.0f, 1.0f); // Red
	else if (name == "Collectible")
		shapeRenderer.FillColor = Color(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
	else
		shapeRenderer.FillColor = Color(1.0f, 1.0f, 1.0f, 1.0f); // White

	// Collider for picking and physics
	entity.AddComponent<Component::CircleCollider2D>(35.0f);

	return entity;
}

// Clear all cached UI label strings (call when entity list changes)
void EditorCore::_invalidateCache() {
	m_cachedDeleteLabels.clear();
	m_cachedCloneLabels.clear();
}

// Change an entity's parent while preventing circular references
void EditorCore::ReparentEntity(EntityId childId, EntityId newParentId) {
	// Can't parent to self
	if (!HasValidWorld() || childId == newParentId) return;

	// Walk up the ancestry chain of the new parent to check for circular parenting
	// If we encounter the child anywhere in the chain, we would create a loop
	EntityId checkId = newParentId;
	while (checkId != 0) {
		if (checkId == childId) {
			LOG_WARNING("Cannot create circular parent relationship");
			return;
		}

		// Move one level up in the hierarchy by following ParentId
		auto checkEntity = m_world->GetEntityManager().GetEntity(checkId);
		auto* checkTransform = checkEntity.GetComponent<Component::Transform>();

		// Continue up the chain if transform exists otherwise stop at root
		checkId = checkTransform ? checkTransform->ParentId : 0;
	}

	// At this point we are sure there is no circular parenting happening
	// Grab the child entity from the world
	auto entity = m_world->GetEntityManager().GetEntity(childId);

	// Get its Transform component which stores the ParentId
	auto* transform = entity.GetComponent<Component::Transform>();

	// If the transform exists we can safely update its ParentId to the new parent
	if (transform) {
		transform->ParentId = newParentId;
		LOG_INFO("Reparented entity " << childId << " to " << newParentId);
	}
}

// Helper to find all entities that have this entity as their parent
// We need this for recursive deletion, otherwise we'd orphan child entities when deleting parents
// Just loops through all entities and checks if their ParentId matches what we're looking for
std::vector<EntityId> EditorCore::_getChildren(EntityId parentId) const {
	std::vector<EntityId> children;
	auto allEntities = m_world->GetEntityManager().GetAllEntities();

	for (auto id : allEntities) {
		auto entity = m_world->GetEntityManager().GetEntity(id);
		auto* transform = entity.GetComponent<Component::Transform>();
		if (transform && transform->ParentId == parentId) {
			children.push_back(id);
		}
	}

	return children;
}

// ImGui needs unique string IDs for every button, otherwise it gets confused about which one we clicked
// We cache these strings per entity so we're not allocating new strings every single frame
const std::string& EditorCore::_getDeleteLabel(const EntityId id) const {
	auto it = m_cachedDeleteLabels.find(id);
	if (it == m_cachedDeleteLabels.end()) {
		// The ## part is ImGui's way of hiding the ID from the visible button text
		std::string label = "Delete##" + std::to_string(id);
		it = m_cachedDeleteLabels.insert({ id, label }).first;
	}
	return it->second;
}

// We keep separate caches because the same entity could have both delete and clone buttons visible
// Caching these saves us from doing string concatenation and allocation every frame
const std::string& EditorCore::_getCloneLabel(const EntityId id) const {
	auto it = m_cachedCloneLabels.find(id);
	if (it == m_cachedCloneLabels.end()) {
		std::string label = "Clone##" + std::to_string(id);
		it = m_cachedCloneLabels.insert({ id, label }).first;
	}
	return it->second;
}

// Tracks whether each entity's tree node is expanded or collapsed in the hierarchy
// ImGui needs a persistent bool reference to remember the expand/collapse state between frames
const bool& EditorCore::_getCollapsedHeaderBool(const EntityId id) const {
	auto it = m_cachedCollapsedHeaders.find(id);
	if (it == m_cachedCollapsedHeaders.end()) {
		// We store these in a map so each entity remembers its own state independently
		it = m_cachedCollapsedHeaders.insert({ id, false }).first;
	}
	return it->second;
}
