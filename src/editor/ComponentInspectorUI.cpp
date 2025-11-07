/* Start Header *****************************************************************/
/*!
\file   ComponentInspectorUI.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements unified component rendering UI for both entity inspection and prefab editing.
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

// Set fonts used across property rendering
void ComponentUI::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    // Store main font pointer
    m_mainFont = mainFont;
    // Store bold font pointer
    m_boldFont = boldFont;
    // Store symbols font pointer
    m_symbolsFont = symbolsFont;
}

// Render Local Transform component properties
void ComponentUI::RenderLocalTransform(nlohmann::json& data) {
    // Begin a unified property section with labels
    EditorUI::BeginPropertySection({ "Local Rotation", "Local Position", "Local Scale" });
    // Render quaternion controls for rotation
    EditorUI::RenderQuaternionRow("Local Rotation", data["Rotation"], "X", "Y", "Z", "W", 0.1f);
    // Render 3D vector controls for position
    EditorUI::RenderVector3DRow("Local Position", data["Position"], "X", "Y", "Z", 1.0f);
    // Render 3D vector controls for scale
    EditorUI::RenderVector3DRow("Local Scale", data["Scale"], "X", "Y", "Z", 0.01f);
    // End property section
    EditorUI::EndPropertySection();
}

// Render Sprite Renderer 2D component properties
void ComponentUI::RenderSpriteRenderer2D(nlohmann::json& data) {
    // Begin a property section with sprite related labels
    EditorUI::BeginPropertySection({ "Sprite", "Color", "Tiling", "Offset" });
    // Read texture path from JSON if available
    std::string texPath = data.value("TexturePath", "");
    // Show sprite label for current texture
    ImGui::Text("Sprite");
    // Place the filename on the same row
    ImGui::SameLine();
    // Align filename text to start where input boxes begin
    ImGui::SetCursorPosX(EditorUI::GetContentStartX());
    // Show filename when a path exists
    if (!texPath.empty()) {
        ImGui::TextDisabled("%s", std::filesystem::path(texPath).filename().string().c_str());
    }
    // Show numeric texture id when path is missing
    else {
        uint32_t tid = data.value("TextureId", 0);
        ImGui::TextDisabled("TextureId: %u", tid);
    }
    // Track whether a valid texture was dropped
    bool dropped = false;
    // Begin drag-drop target to accept texture assets dropped from the browser
    if (ImGui::BeginDragDropTarget()) {
        // Accept only payloads tagged as "ASSET_PATH" (file path string)
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            // Read path data from payload
            std::string droppedPath = static_cast<const char*>(payLoad->Data);
            // Get extension string from dropped path
            auto ext = std::filesystem::path(droppedPath).extension().string();
            // Convert extension to lowercase for comparison
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            // Check supported image formats
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                // Request texture from resource manager
                auto tex = RM.Get<Texture>(droppedPath);
                // Validate texture pointer
                if (tex) {
                    // Update texture id in JSON
                    data["TextureId"] = static_cast<uint32_t>(tex->ID());
                    // Update texture path in JSON
                    data["TexturePath"] = droppedPath;
                    // Store width in JSON
                    data["Width"] = tex->Width();
                    // Store height in JSON
                    data["Height"] = tex->Height();
                    // Mark successful drop
                    dropped = true;
                    // Log success with path and id
                    LOG_INFO("Dropped texture: " << droppedPath << ", id=" << tex->ID());
                }
                // Handle texture load failure
                else {
                    // Log error message with path
                    LOG_ERROR("Failed to load dropped texture: " << droppedPath);
                }
            }
        }
        // End drag drop target scope
        ImGui::EndDragDropTarget();
    }
    // Show status message when texture updated
    if (dropped) {
        // Continue on the same row for status text
        ImGui::SameLine();
        // Show success text with green color
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Texture updated");
    }
    // Render color property controls for sprite tint (RGBA)
    EditorUI::RenderColorProperty("Color##Sprite", data["Color"]);
    // Render tiling vector controls
    EditorUI::RenderVector2DRow("Tiling##Sprite", data["Tiling"], "X", "Y", 0.1f);
    // Render offset vector controls
    EditorUI::RenderVector2DRow("Offset##Sprite", data["Offset"], "X", "Y", 0.1f);
    // End the property section for sprite
    EditorUI::EndPropertySection();
}

// Render Rigidbody 2D component properties
void ComponentUI::RenderRigidbody2D(nlohmann::json& data) {
    // Begin a property section for physics parameters
    EditorUI::BeginPropertySection({ "Mass", "Inverse Mass", "Linear Damping", "Angular Damping", "Gravity Scale", "Flags" });
    // Render mass value in kilograms
    EditorUI::RenderFloatRow("Mass", "kg", data, "Mass", 0.1f);
    // Render inverse mass value
    EditorUI::RenderFloatRow("Inverse Mass", "1/kg", data, "InverseMass", 0.1f);
    // Render linear damping value
    EditorUI::RenderFloatRow("Linear Damping", "", data, "LinearDamping", 0.01f);
    // Render angular damping value
    EditorUI::RenderFloatRow("Angular Damping", "", data, "AngularDamping", 0.01f);
    // Render gravity scale value
    EditorUI::RenderFloatRow("Gravity Scale", "", data, "GravityScale", 0.1f);
    // Render integer flags for custom state
    EditorUI::RenderIntProperty("Flags", data, "Flags");
    // End the physics property section
    EditorUI::EndPropertySection();
}

// Render Circle Collider 2D component properties
void ComponentUI::RenderCircleCollider2D(nlohmann::json& data) {
    // Begin a property section for collider parameters
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Radius", "Layer Mask" });
    // Show label for trigger state
    ImGui::Text("Is Trigger");
    // Keep checkbox on the same row as label
    ImGui::SameLine();
    // Align checkbox to unified content start position
    ImGui::SetCursorPosX(EditorUI::GetContentStartX());
    // Read flags integer from JSON
    int flags = data.value("Flags", 0);
    // Compute trigger state from flag bit
    bool isTrigger = (flags & 0x1) != 0;
    // Toggle flag bit when checkbox changes
    if (ImGui::Checkbox("##IsTriggerCircle", &isTrigger)) {
        // Set or clear the trigger bit
        flags = isTrigger ? (flags | 0x1) : (flags & ~0x1);
        // Write updated flags into JSON
        data["Flags"] = flags;
    }
    // Render offset vector controls for collider center
    EditorUI::RenderVector2DRow("Offset##Circle", data["Offset"], "X", "Y", 1.0f);
    // Render radius float control in pixels
    EditorUI::RenderFloatRow("Radius##Circle", "px", data, "Radius", 1.0f);
    // Render integer property for layer mask
    EditorUI::RenderIntProperty("Layer Mask##Circle", data, "LayerMask");
    // End the collider property section
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderBoxCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Half Extents", "Rotation", "Layer Mask" });

    ImGui::Text("Is Trigger");
    ImGui::SameLine();
    // Align checkbox to unified content start
    ImGui::SetCursorPosX(EditorUI::GetContentStartX());
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

void ComponentUI::RenderShapeCircle2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Radius", "Offset", "Color", "Thickness", "Filled" });

    EditorUI::RenderFloatRow("Radius##ShapeCircle2D", "px", data, "Radius", 1.0f);
    EditorUI::RenderVector2DRow("Offset##ShapeCircle2D", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderColorProperty("Color##ShapeCircle2D", data["Color"]);
    EditorUI::RenderFloatRow("Thickness##ShapeCircle2D", "px", data, "Thickness", 0.1f);

    ImGui::Text("Filled");
    ImGui::SameLine();
    // Align checkbox to unified content start
    ImGui::SetCursorPosX(EditorUI::GetContentStartX());
    bool filled = data.value("Filled", false);
    if (ImGui::Checkbox("##FilledShapeCircle2D", &filled)) {
        data["Filled"] = filled;
    }

    EditorUI::EndPropertySection();
}

void ComponentUI::RenderShapeBox2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Half Extents", "Offset", "Color", "Thickness", "Filled" });

    EditorUI::RenderVector2DRow("Half Extents##ShapeBox2D", data["HalfExtents"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Offset##ShapeBox2D", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderColorProperty("Color##ShapeBox2D", data["Color"]);
    EditorUI::RenderFloatRow("Thickness##ShapeBox2D", "px", data, "Thickness", 0.1f);

    ImGui::Text("Filled");
    ImGui::SameLine();
    // Align checkbox to unified content start
    ImGui::SetCursorPosX(EditorUI::GetContentStartX());
    bool filled = data.value("Filled", false);
    if (ImGui::Checkbox("##FilledShapeBox2D", &filled)) {
        data["Filled"] = filled;
    }

    EditorUI::EndPropertySection();
}

void ComponentUI::RenderShapeLine2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Point A", "Point B", "Thickness", "Color" });

    EditorUI::RenderVector2DRow("Point A##ShapeLine2D", data["A"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Point B##ShapeLine2D", data["B"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Thickness##ShapeLine2D", "px", data, "Thickness", 0.1f);
    EditorUI::RenderColorProperty("Color##ShapeLine2D", data["Color"]);

    EditorUI::EndPropertySection();
}

void ComponentUI::RenderCamera3D(nlohmann::json& data) {
    // Compute alignment based on all possible labels for consistent content anchoring
    EditorUI::BeginPropertySection({ "Mode", "Near Plane", "Far Plane", "Aspect Ratio", "FOV", "Ortho Size" });

    // Row: Active + Perspective toggles
    EditorUI::RenderCheckboxRow("Mode", data, "Active", "Active", "UsePerspective", "Perspective");

    bool usePerspective = data.value("UsePerspective", false);
    // Projection-specific field
    if (usePerspective) {
        EditorUI::RenderFloatRow("FOV", "deg", data, "FOV", 0.5f);
    } else {
        EditorUI::RenderFloatRow("Ortho Size", "units", data, "OrthoSize", 0.5f);
    }

    // Common camera planes and aspect ratio
    EditorUI::RenderFloatRow("Near Plane", "", data, "NearPlane", 0.01f);
    EditorUI::RenderFloatRow("Far Plane", "", data, "FarPlane", 0.1f);
    EditorUI::RenderFloatRow("Aspect Ratio", "w/h", data, "AspectRatio", 0.01f);

    EditorUI::EndPropertySection();
}
