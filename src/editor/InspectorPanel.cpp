/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.cpp
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Implements the unified InspectorPanel that adapts to selection context:
- Entity mode: Shows components of a selected entity instance
- Prefab mode: Shows components of a prefab template (editing the .prefab file)
*/
/* End Header *******************************************************************/

#include "../editor/InspectorPanel.h"
#include "../editor/ComponentInspectorUI.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "../editor/EditorUIHelpers.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <unordered_set>

namespace {
    template <typename T>
    bool ApplyComponentIfMatch(ECS::World* world,
        ECS::Entity entity,
        const std::string& typeName,
        const std::string& expectedName,
        nlohmann::json& componentEntry) {
        // Normalize short type names (e.g. "SpriteRenderer2D") to
        // fully-qualified names (e.g. "ECS::Components::SpriteRenderer2D")
        const std::string prefix = "ECS::Components::";
        const bool hasPrefix = typeName.rfind(prefix, 0) == 0;
        const std::string normalizedType = hasPrefix ? typeName : (prefix + typeName);

        if (normalizedType == expectedName) {
            if (world->Has<T>(entity)) {
                auto& comp = world->Get<T>(entity);
                from_json(componentEntry["Data"], comp);
            }
            return true;
        }
        return false;
    }
}

// Helper to normalize component type names to canonical form
namespace {
    inline std::string NormalizeTypeName(const std::string& tn) {
        static const std::string kPrefix = "ECS::Components::";
        if (tn.rfind(kPrefix, 0) == 0) return tn;
        return kPrefix + tn;
    }
    inline std::string ShortTypeName(const std::string& tn) {
        static const std::string kPrefix = "ECS::Components::";
        if (tn.rfind(kPrefix, 0) == 0) return tn.substr(kPrefix.size());
        return tn;
    }
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
void InspectorPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    m_componentUI.Initialize(mainFont, boldFont, symbolsFont);
}

void InspectorPanel::SetWorld(ECS::World* world) {
    m_world = world;
    ClearSelection();
}

// -------------------------------------------------------------------------
// Selection Management
// -------------------------------------------------------------------------
// Inspect an entity by its ID
void InspectorPanel::InspectEntity(EntityId id) {
    m_mode = InspectionMode::Entity;
    m_entityId = id;
    m_prefabPath.clear();
}

// Inspect a prefab file
void InspectorPanel::InspectPrefab(const std::string& path) {
    m_mode = InspectionMode::Prefab;
    m_prefabPath = path;
    m_entityId = 0;
    _loadPrefabData();
}

// Clear current selection
void InspectorPanel::ClearSelection() {
    m_mode = InspectionMode::None;
    m_entityId = 0;
    m_prefabPath.clear();
    m_prefabData.clear();
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------
void InspectorPanel::Render(float) {
    ImGui::PushFont(m_mainFont);

    const char* windowTitle = (m_mode == InspectionMode::Prefab) ? "Prefab Editor" : "Property Editor";
    ImGui::Begin(windowTitle);

    if (m_mode == InspectionMode::None) {
        ImGui::TextDisabled("No selection");
    }
    else if (m_mode == InspectionMode::Entity) {
        _renderEntityInspector();
    }
    else if (m_mode == InspectionMode::Prefab) {
        _renderPrefabInspector();
    }

    _renderStatusBar();

    ImGui::End();
    ImGui::PopFont();
}

// -------------------------------------------------------------------------
// Entity Inspector
// -------------------------------------------------------------------------
// Render the entity inspector with all components
void InspectorPanel::_renderEntityInspector() {
    if (!m_world) {
        ImGui::TextDisabled("No world available");
        return;
    }

    ECS::Entity entity = m_world->Resolve(m_entityId);
    if (entity.IsNull() || !m_world->IsAlive(entity)) {
        ImGui::TextDisabled("Entity no longer exists");
        return;
    }

    _renderEntityHeader(entity);
    
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    _renderEntityComponents(entity);
    
    ImGui::Dummy(ImVec2(0, 4));
    _renderAddComponentButton(entity);
}

// Render entity header with name
void InspectorPanel::_renderEntityHeader(ECS::Entity entity) {
    ImGui::PushFont(m_boldFont);
    if (m_world->Has<ECS::Components::Name>(entity)) {
        const auto& name = m_world->Get<ECS::Components::Name>(entity);
        ImGui::Text("%s", name.Value);
    }
    else {
        ImGui::Text("Entity %u", m_entityId);
    }
    ImGui::PopFont();

    ImGui::Text("ID: %u", m_entityId);
}

// Render all components for the entity
void InspectorPanel::_renderEntityComponents(ECS::Entity entity) {
    ImGui::BeginChild("ComponentList", ImVec2(0, 0), false, ImGuiWindowFlags_None);

    // Get the content width once for proper horizontal scrolling
    const float contentWidth = EditorUI::GetContentWidth();

    auto renderComponent = [&](const char* label, auto renderFn) {
        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            renderFn();
            ImGui::Dummy(ImVec2(contentWidth, 0.0f));
            ImGui::Spacing();
        }
    };

    // Render each component type
    if (m_world->Has<ECS::Components::LocalTransform>(entity)) {
        renderComponent("Local Transform", [&]() {
            auto& comp = m_world->Get<ECS::Components::LocalTransform>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderLocalTransform(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::SpriteRenderer2D>(entity)) {
        renderComponent("Sprite Renderer 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::SpriteRenderer2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderSpriteRenderer2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::Rigidbody2D>(entity)) {
        renderComponent("Rigidbody 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::Rigidbody2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderRigidbody2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::LinearVelocity2D>(entity)) {
        renderComponent("Linear Velocity 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::LinearVelocity2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderLinearVelocity2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::AngularVelocity2D>(entity)) {
        renderComponent("Angular Velocity 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::AngularVelocity2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderAngularVelocity2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::CircleCollider2D>(entity)) {
        renderComponent("Circle Collider 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::CircleCollider2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderCircleCollider2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::BoxCollider2D>(entity)) {
        renderComponent("Box Collider 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::BoxCollider2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderBoxCollider2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::ShapeCircle2D>(entity)) {
        renderComponent("Shape Circle 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::ShapeCircle2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderShapeCircle2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::ShapeBox2D>(entity)) {
        renderComponent("Shape Box 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::ShapeBox2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderShapeBox2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::ShapeLine2D>(entity)) {
        renderComponent("Shape Line 2D", [&]() {
            auto& comp = m_world->Get<ECS::Components::ShapeLine2D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderShapeLine2D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::Camera3D>(entity)) {
        renderComponent("Camera 3D", [&]() {
            auto& comp = m_world->Get<ECS::Components::Camera3D>(entity);
            nlohmann::json j;
            to_json(j, comp);
            m_componentUI.RenderCamera3D(j);
            from_json(j, comp);
        });
    }

    if (m_world->Has<ECS::Components::AudioSource>(entity)) {
        renderComponent("Audio Source", [&]() {
            auto& comp = m_world->Get<ECS::Components::AudioSource>(entity);
            nlohmann::json j;
            to_json(j, comp);                     // Component -> JSON
            m_componentUI.RenderAudioSource(j);   // Draw + edit UI
            from_json(j, comp);                   // JSON -> component
            });
    }

    ImGui::EndChild();
}

// Render add component button
void InspectorPanel::_renderAddComponentButton(ECS::Entity entity) {
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (ImGui::MenuItem("Sprite Renderer 2D") && !m_world->Has<ECS::Components::SpriteRenderer2D>(entity)) {
            m_world->Add<ECS::Components::SpriteRenderer2D>(entity);
        }
        if (ImGui::MenuItem("Rigidbody 2D") && !m_world->Has<ECS::Components::Rigidbody2D>(entity)) {
            m_world->Add<ECS::Components::Rigidbody2D>(entity);
        }
        if (ImGui::MenuItem("Circle Collider 2D") && !m_world->Has<ECS::Components::CircleCollider2D>(entity)) {
            m_world->Add<ECS::Components::CircleCollider2D>(entity);
        }
        if (ImGui::MenuItem("Box Collider 2D") && !m_world->Has<ECS::Components::BoxCollider2D>(entity)) {
            m_world->Add<ECS::Components::BoxCollider2D>(entity);
        }
        if (ImGui::MenuItem("Audio Source") &&!m_world->Has<ECS::Components::AudioSource>(entity))
        {
            m_world->Add<ECS::Components::AudioSource>(entity);
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Prefab Inspector
// -------------------------------------------------------------------------
// Render the prefab inspector
void InspectorPanel::_renderPrefabInspector() {
    if (m_prefabPath.empty()) {
        ImGui::TextDisabled("No prefab loaded");
        return;
    }

    _renderPrefabHeader();
    
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));

    _renderPrefabComponents();
    
    ImGui::Dummy(ImVec2(0, 4));
    _renderPrefabActions();
}

// Render prefab header
void InspectorPanel::_renderPrefabHeader() {
    ImGui::PushFont(m_boldFont);
    std::string filename = std::filesystem::path(m_prefabPath).filename().string();
    ImGui::Text("%s", filename.c_str());
    ImGui::PopFont();

    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", m_prefabPath.c_str());
}

// Render prefab components
void InspectorPanel::_renderPrefabComponents() {
    if (!m_prefabData.contains("Components")) {
        ImGui::TextDisabled("No components in prefab");
        return;
    }

    ImGui::BeginChild("PrefabComponentList", ImVec2(0, 0), false, ImGuiWindowFlags_None);

    const float contentWidth = EditorUI::GetContentWidth();

    size_t idx = 0;
    for (auto& compEntry : m_prefabData["Components"]) {
        std::string typeName = compEntry.value("Type", "Unknown");
        std::string shortName = ShortTypeName(typeName);
        std::string headerLabel = shortName + "##comp_" + std::to_string(idx);

        if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            _renderPrefabComponent(typeName, compEntry["Data"]);
            ImGui::Dummy(ImVec2(contentWidth, 0.0f));
            ImGui::Spacing();
        }
        ++idx;
    }

    ImGui::EndChild();
}

// Render a single prefab component
void InspectorPanel::_renderPrefabComponent(const std::string& typeName, nlohmann::json& componentData) {
    std::string normalized = NormalizeTypeName(typeName);

    // Render based on component type
    if (normalized == "ECS::Components::LocalTransform") {
        m_componentUI.RenderLocalTransform(componentData);
    }
    else if (normalized == "ECS::Components::SpriteRenderer2D") {
        m_componentUI.RenderSpriteRenderer2D(componentData);
    }
    else if (normalized == "ECS::Components::Rigidbody2D") {
        m_componentUI.RenderRigidbody2D(componentData);
    }
    else if (normalized == "ECS::Components::LinearVelocity2D") {
        m_componentUI.RenderLinearVelocity2D(componentData);
    }
    else if (normalized == "ECS::Components::AngularVelocity2D") {
        m_componentUI.RenderAngularVelocity2D(componentData);
    }
    else if (normalized == "ECS::Components::CircleCollider2D") {
        m_componentUI.RenderCircleCollider2D(componentData);
    }
    else if (normalized == "ECS::Components::BoxCollider2D") {
        m_componentUI.RenderBoxCollider2D(componentData);
    }
    else if (normalized == "ECS::Components::ShapeCircle2D") {
        m_componentUI.RenderShapeCircle2D(componentData);
    }
    else if (normalized == "ECS::Components::ShapeBox2D") {
        m_componentUI.RenderShapeBox2D(componentData);
    }
    else if (normalized == "ECS::Components::ShapeLine2D") {
        m_componentUI.RenderShapeLine2D(componentData);
    }
    else if (normalized == "ECS::Components::Camera3D") {
        m_componentUI.RenderCamera3D(componentData);
    }
    else {
        ImGui::TextDisabled("(Component UI not implemented)");
    }
}

// Render prefab action buttons
void InspectorPanel::_renderPrefabActions() {
    ImGui::Separator();
    
    if (ImGui::Button("Save Changes")) {
        _savePrefabData();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Revert")) {
        _loadPrefabData();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Apply to Instances")) {
        _applyPrefabToInstances();
    }
}

// -------------------------------------------------------------------------
// Prefab Data Management
// -------------------------------------------------------------------------
// Load prefab data from file
void InspectorPanel::_loadPrefabData() {
    if (m_prefabPath.empty()) return;

    try {
        std::ifstream file(m_prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open prefab: " << m_prefabPath);
            m_statusMessage = "Failed to load prefab";
            m_statusTimer = 3.0f;
            return;
        }

        file >> m_prefabData;
        file.close();

        LOG_INFO("Loaded prefab: " << std::filesystem::path(m_prefabPath).filename().string());
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to parse prefab: " << e.what());
        m_statusMessage = "Failed to parse prefab";
        m_statusTimer = 3.0f;
    }
}

// Save prefab data to file
void InspectorPanel::_savePrefabData() {
    if (m_prefabPath.empty()) return;

    try {
        std::ofstream file(m_prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot write to prefab: " << m_prefabPath);
            m_statusMessage = "Failed to save prefab";
            m_statusTimer = 3.0f;
            return;
        }

        file << m_prefabData.dump(2);
        file.close();

        LOG_INFO("Saved prefab: " << std::filesystem::path(m_prefabPath).filename().string());
        m_statusMessage = "Prefab saved";
        m_statusTimer = 3.0f;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to save prefab: " << e.what());
        m_statusMessage = "Failed to save prefab";
        m_statusTimer = 3.0f;
    }
}

// Apply prefab changes to all instances in the scene
void InspectorPanel::_applyPrefabToInstances() {
    if (!m_world || m_prefabPath.empty()) return;

    std::string normalizedPath = std::filesystem::path(m_prefabPath).lexically_normal().string();
    int updateCount = 0;

    m_world->Each<ECS::Components::PrefabLink>([&](ECS::Entity entity, const ECS::Components::PrefabLink& link) {
        if (link.getPath() == normalizedPath) {
            _applyPrefabDataToEntity(entity);
            updateCount++;
        }
        });

    LOG_INFO("Applied prefab to " << updateCount << " instances");
    m_statusMessage = "Applied to " + std::to_string(updateCount) + " instances";
    m_statusTimer = 3.0f;
}

// Apply prefab data to a specific entity
void InspectorPanel::_applyPrefabDataToEntity(ECS::Entity entity) {
    if (!m_prefabData.contains("Components")) return;

    for (auto& compEntry : m_prefabData["Components"]) {
        std::string typeName = compEntry.value("Type", "");
        if (typeName.empty()) continue;

        // Apply component data using template pattern
        ApplyComponentIfMatch<ECS::Components::LocalTransform>(m_world, entity, typeName, "ECS::Components::LocalTransform", compEntry);
        ApplyComponentIfMatch<ECS::Components::SpriteRenderer2D>(m_world, entity, typeName, "ECS::Components::SpriteRenderer2D", compEntry);
        ApplyComponentIfMatch<ECS::Components::Rigidbody2D>(m_world, entity, typeName, "ECS::Components::Rigidbody2D", compEntry);
    }
}

// -------------------------------------------------------------------------
// Status Bar
// -------------------------------------------------------------------------
void InspectorPanel::_renderStatusBar() {
    if (m_statusTimer > 0.0f) {
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        ImGui::Separator();
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }
}
