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

namespace ComponentUI {

// Static font storage (initialized once via Initialize())
static ImFont* s_mainFont = nullptr;
static ImFont* s_boldFont = nullptr;
static ImFont* s_symbolsFont = nullptr;

// === Initialization ===

void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    s_mainFont = mainFont;
    s_boldFont = boldFont;
    s_symbolsFont = symbolsFont;
}

// === Component Rendering Functions ===

// Render Transform component UI
// Position, rotation, scale editing
void RenderTransform(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Local Rotation", "Local Position", "Local Scale" });
    EditorUI::RenderFloatRow("Local Rotation", "R", data, "Rotation", 1.0f);
    EditorUI::RenderVector2DRow("Local Position", data["Position"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Local Scale", data["Scale"], "X", "Y", 0.01f);
    EditorUI::EndPropertySection();
}

// Render SpriteRenderer component UI
// Texture, color tint, flip options, sorting
void RenderSpriteRenderer(nlohmann::json& data) {
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
        // Customize drop target appearance (blue highlight when hovering)
        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.2f, 0.5f, 1.0f, 1.0f));

        // A payload is the data we attach to a drag operation (what we're "carrying")
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = static_cast<const char*>(payLoad->Data);

            // Only accept .png files for sprites
            if (std::filesystem::path(droppedPath).extension() == ".png") {
                data["TexturePath"] = droppedPath;
                data["Sprite"] = droppedPath;
                LOG_INFO("Dropped texture: " << droppedPath);
            }
        }
        ImGui::PopStyleColor();
        ImGui::EndDragDropTarget();
    }

    // Color tint picker
    EditorUI::RenderColorProperty("Color##Sprite", data["Color"]);

    // Flip X/Y checkboxes
    EditorUI::RenderCheckboxRow("Flip", data, "FlipX", "X", "FlipY", "Y");

    // Additional settings (sorting) in collapsible section
    ImGui::Separator();
    ImGui::PushFont(s_boldFont);
    // Make header transparent for cleaner look
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));           // Transparent when not hovered
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));    // Transparent when hovered
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));     // Transparent when active

    if (ImGui::CollapsingHeader("Additional Settings")) {
        ImGui::PopStyleColor(3);  // Pop all 3 color styles
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
// Mass, damping, body type, velocity, etc.
void RenderRigidbody2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Body Type", "Mass", "Linear Damping", "Angular Damping",
                                        "Gravity Scale", "Freeze Rotation", "Linear Velocity",
                                        "Angular Velocity", "Inertia", "Center of Mass" });

    // Body Type dropdown (Dynamic, Kinematic, Static)
    ImGui::Text("Body Type");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());
    ImGui::SetNextItemWidth(100);

    const char* bodyTypes[] = { "Dynamic", "Kinematic", "Static" };
    int currentType = data["BodyType"];
    if (ImGui::Combo("##BodyType", &currentType, bodyTypes, 3)) {
        data["BodyType"] = currentType;
    }

    // Physical properties
    EditorUI::RenderFloatRow("Mass", "kg", data, "Mass", 0.1f);
    EditorUI::RenderFloatRow("Linear Damping", "", data, "LinearDamping", 0.01f);
    EditorUI::RenderFloatRow("Angular Damping", "", data, "AngularDamping", 0.01f);
    EditorUI::RenderFloatRow("Gravity Scale", "", data, "GravityScale", 0.1f);

    // Freeze Rotation constraint
    ImGui::Text("Freeze Rotation");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    bool freezeRot = data.value("FreezeRotation", false);
    if (ImGui::Checkbox("##FreezeRotation", &freezeRot)) {
        data["FreezeRotation"] = freezeRot;
    }
    ImGui::SameLine();
    ImGui::Text("Z");

    // Runtime info (velocity, inertia, etc.) in collapsible section
    ImGui::Separator();
    ImGui::PushFont(s_boldFont);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

    if (ImGui::CollapsingHeader("Info")) {
        ImGui::PopStyleColor(3);
        ImGui::PopFont();
        EditorUI::RenderVector2DRow("Linear Velocity", data["LinearVelocity"], "X", "Y", 1.0f);
        EditorUI::RenderFloatRow("Angular Velocity", "", data, "AngularVelocity", 1.0f);
        EditorUI::RenderFloatRow("Inertia", "", data, "Inertia", 0.1f);
        EditorUI::RenderVector2DRow("Center of Mass", data["CenterOfMass"], "X", "Y", 0.1f);
    }
    else {
        ImGui::PopStyleColor(3);
        ImGui::PopFont();
    }

    EditorUI::EndPropertySection();
}

// Render CircleCollider2D component UI
// Trigger flag, offset, radius, collision layer
void RenderCircleCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Radius", "Layer" });

    // Is Trigger checkbox
    ImGui::Text("Is Trigger");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    bool isTrigger = data.value("IsTrigger", false);
    if (ImGui::Checkbox("##IsTriggerCircle", &isTrigger)) {
        data["IsTrigger"] = isTrigger;
    }

    EditorUI::RenderVector2DRow("Offset##Circle", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Radius##Circle", "px", data, "Radius", 1.0f);
    EditorUI::RenderIntProperty("Layer##Circle", data, "Layer");

    EditorUI::EndPropertySection();
}

// Render BoxCollider2D component UI
// Trigger flag, offset, size, collision layer
void RenderBoxCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Size", "Layer" });

    // Is Trigger checkbox
    ImGui::Text("Is Trigger");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());

    bool isTrigger = data.value("IsTrigger", false);
    if (ImGui::Checkbox("##IsTriggerBox", &isTrigger)) {
        data["IsTrigger"] = isTrigger;
    }

    EditorUI::RenderVector2DRow("Offset##Box", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Size##Box", data["Size"], "X", "Y", 1.0f);
    EditorUI::RenderIntProperty("Layer##Box", data, "Layer");

    EditorUI::EndPropertySection();
}

// Render LineRenderer component UI
// Start point, end point, thickness, color
void RenderLineRenderer(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Start", "End", "Thickness", "Color" });

    EditorUI::RenderVector2DRow("Start", data["Start"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("End", data["End"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Thickness", "px", data, "Thickness", 0.1f);
    EditorUI::RenderColorProperty("Color##Line", data["Color"]);

    EditorUI::EndPropertySection();
}

// Render ShapeRenderer2D component UI
// Shape type (rectangle, circle, polygon), colors, outline
void RenderShapeRenderer2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Shape Type", "Size", "Radius", "Fill Color", "Outline Color", "Thickness" });

    // Shape Type dropdown (Rectangle, Circle, Polygon)
    ImGui::Text("Shape Type");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetCurrentLabelOffset());
    ImGui::SetNextItemWidth(100);

    const char* shapeTypes[] = { "Rectangle", "Circle", "Polygon" };
    int currentShape = data.value("Type", 0);
    if (ImGui::Combo("##ShapeType", &currentShape, shapeTypes, 3)) {
        data["Type"] = currentShape;
    }

    // Shape-specific properties based on selected type
    if (currentShape == 0) {  // Rectangle
        EditorUI::RenderVector2DRow("Size##Rectangle", data["Size"], "X", "Y", 1.0f);
    }
    else if (currentShape == 1) {  // Circle
        EditorUI::RenderFloatRow("Radius##Circle2", "px", data, "Radius", 1.0f);
    }
    else if (currentShape == 2) {  // Polygon
        ImGui::PushFont(s_boldFont);
        ImGui::Text("Points");
        ImGui::PopFont();

        auto& points = data["Points"];
        float maxWidth = ImGui::CalcTextSize("Point 88").x;  // Width for widest expected label

        // Display each point with X/Y fields
        for (size_t i = 0; i < points.size(); i++) {
            ImGui::PushID(static_cast<int>(i));

            // Fixed-width label so all X/Y fields align vertically
            std::string label = "Point " + std::to_string(i);
            ImGui::Text("%s", label.c_str());
            float currentWidth = ImGui::CalcTextSize(label.c_str()).x;
            if (currentWidth < maxWidth) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (maxWidth - currentWidth));
            }

            EditorUI::BeginPropertySection({ "Point" });
            EditorUI::RenderVector2DRow("##Point", points[i], "X", "Y", 1.0f);
            EditorUI::EndPropertySection();

            // Delete point button (red X icon)
            ImGui::SameLine();
            ImGui::PushFont(s_symbolsFont);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));                    // Transparent background
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f)); // Subtle hover
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));  // Subtle active
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));          // Red text/icon

            if (ImGui::SmallButton("\xEE\x97\x89")) {  // X icon
                points.erase(points.begin() + i);
                ImGui::PopStyleColor(4);
                ImGui::PopFont();
                ImGui::PopID();
                break;  // Exit loop since we modified the array
            }
            ImGui::PopStyleColor(4);
            ImGui::PopFont();
            ImGui::PopID();
        }

        // Add new point button
        if (ImGui::Button("Add Point")) {
            points.push_back({ {"X", 0.0f}, {"Y", 0.0f} });
        }

        // Closed polygon checkbox
        ImGui::SameLine();
        bool closed = data.value("Closed", true);
        if (ImGui::Checkbox("Closed##PolygonClosed", &closed)) {
            data["Closed"] = closed;
        }
    }

    // Color properties (shared by all shape types)
    ImGui::Separator();
    ImGui::PushFont(s_boldFont);
    ImGui::Text("Colors");
    ImGui::PopFont();
    EditorUI::RenderColorProperty("Fill Color##Shape", data["FillColor"]);
    EditorUI::RenderColorProperty("Outline Color##Shape", data["OutlineColor"]);
    EditorUI::RenderFloatRow("\" Thickness", "px", data, "OutlineThickness", 0.1f);

    EditorUI::EndPropertySection();
}

} // namespace ComponentUI