/* Start Header *****************************************************************/
/*!
\file   ComponentInspectorUI.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements unified component rendering UI for both entity inspection and prefab editing.

Features:
- Reusable component rendering functions for all component types
- JSON-based interface works with both live entities and prefab files
- Bidirectional conversion between C++ components and JSON representation
- Consistent UI layout across entity inspector and prefab editor

References:
- ImGui documentation for UI widgets and styling
- nlohmann/json for data manipulation
- EditorUIHelpers for property rendering widgets
*/
/* End Header *******************************************************************/

#include "../editor/ComponentInspectorUI.h"
#include "../editor/EditorUIHelpers.h"
#include "core/Logger.h"
#include <imgui.h>
#include <filesystem>
#include "ecs/Components.h"
#include "serialization/EntitySerializer.h"
#include "services/ResourceManager.h"
#include <algorithm>

// Set up fonts for UI rendering
void ComponentUI::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

// Render LocalTransform component UI (was RenderTransform)
// Allows editing of rotation, position and scale
void ComponentUI::RenderLocalTransform(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Local Rotation", "Local Position", "Local Scale" });
    // Match serializer schema: Position/Scale are 3D vectors; Rotation is quaternion
    EditorUI::RenderQuaternionRow("Local Rotation", data["Rotation"], "X", "Y", "Z", "W", 0.1f);
    EditorUI::RenderVector3DRow("Local Position", data["Position"], "X", "Y", "Z", 1.0f);
    EditorUI::RenderVector3DRow("Local Scale", data["Scale"], "X", "Y", "Z", 0.01f);
    EditorUI::EndPropertySection();
}

// Render SpriteRenderer2D component UI (was RenderSpriteRenderer)
// Includes sprite texture, color tint, flip options and sorting info
void ComponentUI::RenderSpriteRenderer2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Sprite", "Color", "Tiling", "Offset" });

    // Sprite texture display with drag-and-drop support; stores TextureId per serializer schema
    std::string texPath = data.value("TexturePath", "");
    ImGui::Text("Sprite");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    // Display current texture filename if known, otherwise show TextureId
    if (!texPath.empty()) {
        ImGui::TextDisabled("%s", std::filesystem::path(texPath).filename().string().c_str());
    }
    else {
        uint32_t tid = data.value("TextureId", 0);
        ImGui::TextDisabled("TextureId: %u", tid);
    }

    // Make the text a drop target for dragging textures from asset browser
    if (ImGui::BeginDragDropTarget()) {
        // Accept asset paths; use default ImGui drag-drop highlight (yellow)
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = static_cast<const char*>(payLoad->Data);

            // Accept common image extensions
            auto ext = std::filesystem::path(droppedPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                auto tex = RM.Get<Texture>(droppedPath);
                if (tex) {
                    data["TextureId"] = static_cast<uint32_t>(tex->ID());
                    data["TexturePath"] = droppedPath; // UI convenience only
                    data["Width"] = tex->Width();       // optional, ignored by serializer
                    data["Height"] = tex->Height();     // optional, ignored by serializer
                    LOG_INFO("Dropped texture: " << droppedPath << ", id=" << tex->ID());
                }
                else {
                    LOG_ERROR("Failed to load dropped texture: " << droppedPath);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Color tint picker per serializer
    EditorUI::RenderColorProperty("Color##Sprite", data["Color"]);
    // Tiling and Offset per serializer
    EditorUI::RenderVector2DRow("Tiling##Sprite", data["Tiling"], "X", "Y", 0.1f);
    EditorUI::RenderVector2DRow("Offset##Sprite", data["Offset"], "X", "Y", 0.1f);

    EditorUI::EndPropertySection();
}

// Render Rigidbody2D component UI
// Shows body type, mass, damping, velocities, constraints and inertia
void ComponentUI::RenderRigidbody2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Mass", "Inverse Mass", "Linear Damping", "Angular Damping", "Gravity Scale", "Flags" });
    EditorUI::RenderFloatRow("Mass", "kg", data, "Mass", 0.1f);
    EditorUI::RenderFloatRow("Inverse Mass", "1/kg", data, "InverseMass", 0.1f);
    EditorUI::RenderFloatRow("Linear Damping", "", data, "LinearDamping", 0.01f);
    EditorUI::RenderFloatRow("Angular Damping", "", data, "AngularDamping", 0.01f);
    EditorUI::RenderFloatRow("Gravity Scale", "", data, "GravityScale", 0.1f);

    // Flags as integer; future: expose common bits via checkboxes if needed
    EditorUI::RenderIntProperty("Flags", data, "Flags");
    EditorUI::EndPropertySection();
}

// Render CircleCollider2D component UI
// Allows editing trigger flag, offset, radius and collision layer
void ComponentUI::RenderCircleCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Radius", "Layer Mask" });

    // Map IsTrigger to Flags bit 0 while still showing a simple checkbox
    ImGui::Text("Is Trigger");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());
    int flags = data.value("Flags", 0);
    bool isTrigger = (flags & 0x1) != 0;
    if (ImGui::Checkbox("##IsTriggerCircle", &isTrigger)) {
        flags = isTrigger ? (flags | 0x1) : (flags & ~0x1);
        data["Flags"] = flags;
    }

    EditorUI::RenderVector2DRow("Offset##Circle", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Radius##Circle", "px", data, "Radius", 1.0f);
    EditorUI::RenderIntProperty("Layer Mask##Circle", data, "LayerMask");

    EditorUI::EndPropertySection();
}

// Render BoxCollider2D component UI
// Allows editing trigger flag, offset, size and collision layer
void ComponentUI::RenderBoxCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Half Extents", "Rotation", "Layer Mask" });

    // Map IsTrigger to Flags bit 0
    ImGui::Text("Is Trigger");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());
    int flags = data.value("Flags", 0);
    bool isTrigger = (flags & 0x1) != 0;
    if (ImGui::Checkbox("##IsTriggerBox", &isTrigger)) {
        flags = isTrigger ? (flags | 0x1) : (flags & ~0x1);
        data["Flags"] = flags;
    }

    EditorUI::RenderVector2DRow("Offset##Box", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Half Extents##Box", data["HalfExtents"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Rotation##Box", "deg", data, "Rotation", 1.0f);
    EditorUI::RenderIntProperty("Layer Mask##Box", data, "LayerMask");

    EditorUI::EndPropertySection();
}

// Render ShapeCircle2D component UI
// Allows editing radius, offset, color, thickness and fill options
void ComponentUI::RenderShapeCircle2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Radius", "Offset", "Color", "Thickness", "Filled" });

    EditorUI::RenderFloatRow("Radius", "px", data, "Radius", 1.0f);
    EditorUI::RenderVector2DRow("Offset", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderColorProperty("Color##ShapeCircle", data["Color"]);
    EditorUI::RenderFloatRow("Thickness", "px", data, "Thickness", 0.1f);

    // Filled checkbox
    ImGui::Text("Filled");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());
    bool filled = data.value("Filled", false);
    if (ImGui::Checkbox("##FilledCircle", &filled)) {
        data["Filled"] = filled;
    }

    EditorUI::EndPropertySection();
}

// Render ShapeBox2D component UI
// Allows editing half extents (size), offset, color, thickness and fill options
void ComponentUI::RenderShapeBox2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Half Extents", "Offset", "Color", "Thickness", "Filled" });

    EditorUI::RenderVector2DRow("Half Extents", data["HalfExtents"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Offset", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderColorProperty("Color##ShapeBox", data["Color"]);
    EditorUI::RenderFloatRow("Thickness", "px", data, "Thickness", 0.1f);

    // Filled checkbox
    ImGui::Text("Filled");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());
    bool filled = data.value("Filled", false);
    if (ImGui::Checkbox("##FilledBox", &filled)) {
        data["Filled"] = filled;
    }

    EditorUI::EndPropertySection();
}

// Render ShapeLine2D component UI (was RenderLineRenderer)
// Allows editing start/end points, thickness and color
void ComponentUI::RenderShapeLine2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Point A", "Point B", "Thickness", "Color" });

    // Using "A" and "B" to match the component struct field names
    EditorUI::RenderVector2DRow("Point A", data["A"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Point B", data["B"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Thickness", "px", data, "Thickness", 0.1f);
    EditorUI::RenderColorProperty("Color##ShapeLine", data["Color"]);

    EditorUI::EndPropertySection();
}
