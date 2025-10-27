/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file GameObjectManagement.cpp
 * @brief
 */


#include "../editor/GameObjectManagement.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "services/DebugUI.h"
#include "services/Input.h"
#include <imgui.h>
#include "services/UICommon.h"
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




// Check if World object exists
bool GameObjectEditor::HasValidWorld() const {
    return m_world != nullptr;
}

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

// Find and delete an object by ID
void GameObjectEditor::RemoveGameObject(const EntityId id) {
    // Safety check
    if (!HasValidWorld()) return;

    const auto entity = m_world->GetEntityManager().GetEntity(id);
    m_world->GetEntityManager().DestroyEntity(entity);

    _invalidateCache();
}

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

void GameObjectEditor::ClearAllGameObjects() {
    // Safety check
    if (!HasValidWorld()) return;

    m_world->GetEntityManager().DestroyAllEntities();

    _invalidateCache();
}

void GameObjectEditor::_showGameObjectEditor() {
    // Use config values
    UICommon::ApplyLayout(UICommon::WindowId::DEBUG_EDITOR);

    ImGui::Begin("Game Object Editor");

    // Add new game object section
    ImGui::Text("Create New Object:");
    static char nameBuffer[DebugUIConfig::MAX_OBJECT_NAME_LENGTH];
    if (m_newObjectName.length() < sizeof(nameBuffer)) {
        // Store new object name
        strcpy_s(nameBuffer, m_newObjectName.c_str());
    }

    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        m_newObjectName = nameBuffer;
    }

    // When button is clicked, create the object
    if (ImGui::Button("Add Object") && !m_newObjectName.empty()) {
        AddGameObject(m_newObjectName);
        m_newObjectName = "NewObject"; // Reset
    }

    ImGui::Separator();

    // Quick add buttons for common objects
    ImGui::Text("Quick Add:");
    if (ImGui::Button("Player")) AddGameObject("Player");
    ImGui::SameLine();  // Make the next button appear on the same line instead of below
    if (ImGui::Button("Enemy")) AddGameObject("Enemy");
    ImGui::SameLine();
    if (ImGui::Button("Collectible")) AddGameObject("Collectible");

    ImGui::Separator();

    static char prefabName[128] = "sample-enemy-prefab";
    ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));
    if (ImGui::Button("Load Prefab") && strlen(prefabName) > 0) {
        std::ifstream file("assets/samples/" + std::string(prefabName) + ".prefab");
        if (!file.is_open()) {
            LOG_ERROR("Cannot open file: " << prefabName);
        }
        else {
            try {
                auto entityJson = nlohmann::json::parse(file);
                file.close();

                // Deserialize creates the entity internally
                (void)Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

                _invalidateCache();
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to parse prefab file: " << e.what());
            }
        }
    }

    // Display list of current objects
    const auto entities = m_world->GetEntityManager().GetAllEntities();
    ImGui::Text("Current Objects (%zu):", entities.size());

    // For each object
    for (const auto& entId : entities) {
        auto entity = m_world->GetEntityManager().GetEntity(entId); // Ensure entity is valid

        // Active status
        std::stringstream oss;
        oss << "[" << entity.GetId() << "] " << entity.GetName();
        if (ImGui::CollapsingHeader(oss.str().c_str(), _getCollapsedHeaderBool(entId))) {
            // Delete button for each object
            if (ImGui::SmallButton(_getDeleteLabel(entId).c_str())) {
                RemoveGameObject(entId);
                break;
            }

            // Clone button for each object
            ImGui::SameLine();
            if (ImGui::SmallButton(_getCloneLabel(entId).c_str())) {
                CloneGameObject(entity);
                break;
            }

            ImGui::SeparatorText("Transform");

            // Get pointer to transform component to ensure we're modifying the actual component
            auto* transform = entity.GetComponent<Component::Transform>();
            if (transform) {
                // BEFORE modification
                ImGui::Text("DEBUG: Current Scale: (%.2f, %.2f)", transform->Scale.X, transform->Scale.Y);

                ImGui::Text("Position");
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("X##P" + std::to_string(entId)).c_str(), &transform->Position.X);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Y##P" + std::to_string(entId)).c_str(), &transform->Position.Y);

                ImGui::SetNextItemWidth(100.f);
                ImGui::InputFloat(std::string("Rotation##" + std::to_string(entId)).c_str(), &transform->Rotation);

                ImGui::Text("Scale");
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::InputFloat(std::string("X##S" + std::to_string(entId)).c_str(), &transform->Scale.X)) {
                    // Print when value changes
                    std::cout << "Scale.X changed to: " << transform->Scale.X << std::endl;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::InputFloat(std::string("Y##S" + std::to_string(entId)).c_str(), &transform->Scale.Y)) {
                    std::cout << "Scale.Y changed to: " << transform->Scale.Y << std::endl;
                }
            }
        }
    }
    ImGui::Separator();

    // Clear all buttons
    if (ImGui::Button("Clear All Objects")) {
        ClearAllGameObjects();
    }

    ImGui::End();
}

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

// Clear cached button labels
void GameObjectEditor::_invalidateCache() {
    m_cachedDeleteLabels.clear();
    m_cachedCloneLabels.clear();
}


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

const bool& GameObjectEditor::_getCollapsedHeaderBool(const EntityId id) const {
    auto it = m_cachedCollapsedHeaders.find(id);
    if (it == m_cachedCollapsedHeaders.end()) {
        it = m_cachedCollapsedHeaders.insert({ id, false }).first;
    }
    return it->second;
}
