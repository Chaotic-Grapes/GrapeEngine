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
    EditorUI::RenderFloatRow("Local Rotation", "R", data, "Rotation", 1.0f);         // Single float row
    EditorUI::RenderVector2DRow("Local Position", data["Position"], "X", "Y", 1.0f); // 2D vector row
    EditorUI::RenderVector2DRow("Local Scale", data["Scale"], "X", "Y", 0.01f);      // 2D vector row with small increment
    EditorUI::EndPropertySection();
}

// Render SpriteRenderer2D component UI (was RenderSpriteRenderer)
// Includes sprite texture, color tint, flip options and sorting info
void ComponentUI::RenderSpriteRenderer2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Sprite", "Color", "Flip", "Sorting Layer", "Order in Layer" });

    // Sprite texture path display with drag-and-drop support
    std::string texPath = data.value("TexturePath", "");
    ImGui::Text("Sprite");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    // Display current texture filename or "None"
    ImGui::TextDisabled("%s", texPath.empty() ? "None"
        : std::filesystem::path(texPath).filename().string().c_str());

    // Make the text a drop target for dragging textures from asset browser
    if (ImGui::BeginDragDropTarget()) {
        // Accept asset paths; use default ImGui drag-drop highlight (yellow)
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = static_cast<const char*>(payLoad->Data);

            // Only accept .png files for sprites
            if (std::filesystem::path(droppedPath).extension() == ".png") {
                data["TexturePath"] = droppedPath;
                data["Sprite"] = droppedPath;
                LOG_INFO("Dropped texture: " << droppedPath);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Color tint picker
    EditorUI::RenderColorProperty("Color##Sprite", data["Color"]);

    // Flip X/Y checkboxes
    EditorUI::RenderCheckboxRow("Flip", data, "FlipX", "X", "FlipY", "Y");

    // Additional settings (sorting) in collapsible section
    ImGui::Separator();
    ImGui::PushFont(m_boldFont);

    // Make header transparent for cleaner look
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));        // Transparent when not hovered
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0)); // Transparent when hovered
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));  // Transparent when active

    if (ImGui::CollapsingHeader("Additional Settings")) {
        ImGui::PopStyleColor(3); // Pop all 3 color styles
        ImGui::PopFont();
        // Sorting layer name (for render order grouping)
        EditorUI::RenderTextProperty("Sorting Layer", data, "SortingLayerName");
        // Order in layer (fine-grained sorting within a layer)
        EditorUI::RenderIntProperty("Order in Layer", data, "SortingOrder");
    }
    else {
        // Also pop if header is collapsed
        ImGui::PopStyleColor(3);
        ImGui::PopFont();
    }

    EditorUI::EndPropertySection();
}

// Render Rigidbody2D component UI
// Shows body type, mass, damping, velocities, constraints and inertia
void ComponentUI::RenderRigidbody2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Body Type", "Mass", "Linear Damping", "Angular Damping",
        "Gravity Scale", "Freeze Rotation", "Linear Velocity",
        "Angular Velocity", "Inertia", "Center of Mass" });

    // Body Type dropdown (Dynamic, Kinematic, Static)
    ImGui::Text("Body Type");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());
    ImGui::SetNextItemWidth(100);

    // Stored as an integer index (0 = Dynamic, 1 = Kinematic, 2 = Static)
    const char* bodyTypes[] = { "Dynamic", "Kinematic", "Static" };

    // Retrieve the current body type from JSON data
    // Render a combo box (dropdown) with the body type options
    int currentType = data["BodyType"];
    // "##BodyType" hides the label visually but keeps it unique for ImGui
    if (ImGui::Combo("##BodyType", &currentType, bodyTypes, 3)) {
        // If the user selects a different option, update the JSON data
        data["BodyType"] = currentType;
    }

    // Mass and damping properties
    EditorUI::RenderFloatRow("Mass", "kg", data, "Mass", 0.1f);
    EditorUI::RenderFloatRow("Linear Damping", "", data, "LinearDamping", 0.01f);
    EditorUI::RenderFloatRow("Angular Damping", "", data, "AngularDamping", 0.01f);
    EditorUI::RenderFloatRow("Gravity Scale", "", data, "GravityScale", 0.1f);

    // Freeze rotation constraint
    ImGui::Text("Freeze Rotation");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    // Retrieve the "FreezeRotation" flag from JSON; default to false if missing
    bool freezeRot = data.value("FreezeRotation", false);

    // Render a checkbox for "Freeze Rotation" (hidden label "##FreezeRotation" for ImGui ID)
    if (ImGui::Checkbox("##FreezeRotation", &freezeRot)) {
        // If the user toggles the checkbox, update the JSON data
        data["FreezeRotation"] = freezeRot;
    }
    ImGui::SameLine();
    ImGui::Text("Z");

    // Runtime info (velocity, inertia, etc.) in collapsible section
    ImGui::Separator();
    ImGui::PushFont(m_boldFont);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

    // Render the "Info" collapsing header
    if (ImGui::CollapsingHeader("Info")) {
        ImGui::PopStyleColor(3);
        ImGui::PopFont();

        // Render runtime physics info as read-only or editable properties
        EditorUI::RenderVector2DRow("Linear Velocity", data["LinearVelocity"], "X", "Y", 1.0f);
        EditorUI::RenderFloatRow("Angular Velocity", "", data, "AngularVelocity", 1.0f);
        EditorUI::RenderFloatRow("Inertia", "", data, "Inertia", 0.1f);
        EditorUI::RenderVector2DRow("Center of Mass", data["CenterOfMass"], "X", "Y", 0.1f);
    }
    else {
        // Pop colors and font even if header is collapsed to maintain UI state consistency
        ImGui::PopStyleColor(3);
        ImGui::PopFont();
    }

    EditorUI::EndPropertySection();
}

// Render CircleCollider2D component UI
// Allows editing trigger flag, offset, radius and collision layer
void ComponentUI::RenderCircleCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Radius", "Layer" });

    // Is Trigger checkbox
    ImGui::Text("Is Trigger");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    // Retrieve value from JSON; default to false if missing
    bool isTrigger = data.value("IsTrigger", false);
    if (ImGui::Checkbox("##IsTriggerCircle", &isTrigger)) {
        // Update JSON if user toggled the checkbox
        data["IsTrigger"] = isTrigger;
    }

    // It's always the same thing man
    EditorUI::RenderVector2DRow("Offset##Circle", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Radius##Circle", "px", data, "Radius", 1.0f);
    EditorUI::RenderIntProperty("Layer##Circle", data, "Layer");

    EditorUI::EndPropertySection();
}

// Render BoxCollider2D component UI
// Allows editing trigger flag, offset, size and collision layer
void ComponentUI::RenderBoxCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Size", "Layer" });

    // Is Trigger checkbox
    ImGui::Text("Is Trigger");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    // Same as above
    bool isTrigger = data.value("IsTrigger", false);
    if (ImGui::Checkbox("##IsTriggerBox", &isTrigger)) {
        data["IsTrigger"] = isTrigger;
    }

    EditorUI::RenderVector2DRow("Offset##Box", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Size##Box", data["Size"], "X", "Y", 1.0f);
    EditorUI::RenderIntProperty("Layer##Box", data, "Layer");

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
