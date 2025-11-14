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

// Initialize the component UI with editor fonts for consistent styling.
// Stores font pointers so sections can mix text and iconography.
void ComponentUI::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

// Render the LocalTransform component with rotation, position, and scale.
// Uses compact single-line rows for vectors and quaternions.
void ComponentUI::RenderLocalTransform(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Local Rotation", "Local Position", "Local Scale" });
    EditorUI::RenderQuaternionRow("Local Rotation", data["Rotation"], "X", "Y", "Z", "W", 0.1f);
    EditorUI::RenderVector3DRow("Local Position", data["Position"], "X", "Y", "Z", 1.0f);
    EditorUI::RenderVector3DRow("Local Scale", data["Scale"], "X", "Y", "Z", 0.01f);
    EditorUI::EndPropertySection();
}

// Render the SpriteRenderer2D component and allow texture drag-and-drop.
// Shows color, tiling, and offset alongside the current texture info.
void ComponentUI::RenderSpriteRenderer2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Sprite", "Color", "Tiling", "Offset" });

    // --- uses layout helper for consistency ---
    uint32_t tid = data.value("TextureId", 0);
    std::string valueText = "TextureId: " + std::to_string(tid);

    // Optionally show filename too
    std::string texPath = data.value("TexturePath", "");
    if (!texPath.empty()) {
        valueText += " (" + std::filesystem::path(texPath).filename().string() + ")";
    }

    EditorUI::RenderStaticValueRow("Sprite", valueText);

    bool dropped = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = static_cast<const char*>(payLoad->Data);
            auto ext = std::filesystem::path(droppedPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                auto tex = RM.Get<Texture>(droppedPath);
                if (tex) {
                    data["TextureId"] = static_cast<uint32_t>(tex->ID());
                    data["TexturePath"] = droppedPath;
                    data["Width"] = tex->Width();
                    data["Height"] = tex->Height();
                    dropped = true;
                    LOG_INFO("Dropped texture: " << droppedPath << ", id=" << tex->ID());
                }
                else {
                    LOG_ERROR("Failed to load dropped texture: " << droppedPath);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (dropped) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Texture updated");
    }

    EditorUI::RenderColorProperty("Color##Sprite", data["Color"]);
    EditorUI::RenderVector2DRow("Tiling##Sprite", data["Tiling"], "X", "Y", 0.1f);
    EditorUI::RenderVector2DRow("Offset##Sprite", data["Offset"], "X", "Y", 0.1f);
    EditorUI::EndPropertySection();
}

// Render basic Rigidbody2D properties with units where applicable.
// Keeps damping and gravity scale intuitive and easy to tweak.
void ComponentUI::RenderRigidbody2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Mass", "Inverse Mass", "Linear Damping", "Angular Damping", "Gravity Scale", "Flags" });
    EditorUI::RenderFloatRow("Mass", "kg", data, "Mass", 0.1f);
    EditorUI::RenderFloatRow("Inverse Mass", "1/kg", data, "InverseMass", 0.1f);
    EditorUI::RenderFloatRow("Linear Damping", "", data, "LinearDamping", 0.01f);
    EditorUI::RenderFloatRow("Angular Damping", "", data, "AngularDamping", 0.01f);
    EditorUI::RenderFloatRow("Gravity Scale", "", data, "GravityScale", 0.1f);
    EditorUI::RenderIntProperty("Flags", data, "Flags");
    EditorUI::EndPropertySection();
}

// Render the LinearVelocity2D component in world units per second.
// Keeps controls on a single row for quick adjustments.
void ComponentUI::RenderLinearVelocity2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Velocity" });
    EditorUI::RenderVector2DRow("Velocity##Linear", data["Value"], "X", "Y", 1.0f);
    EditorUI::EndPropertySection();
}

// Render the AngularVelocity2D component expressed in degrees per second.
// Uses a drag control with a reasonable default step.
void ComponentUI::RenderAngularVelocity2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Angular Velocity" });
    EditorUI::RenderFloatRow("Angular Velocity##Angular", "deg/s", data, "Value", 0.5f);
    EditorUI::EndPropertySection();
}

// Render a CircleCollider2D with trigger flag and geometric properties.
// Writes flag updates back to JSON when the checkbox changes.
void ComponentUI::RenderCircleCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Radius", "Layer Mask" });

    // Extract current flag value
    int flags = data.value("Flags", 0);
    bool isTrigger = (flags & 0x1) != 0;

    // Render checkbox and get if it changed
    if (EditorUI::RenderCheckboxPropertyReturn("Is Trigger##Circle", isTrigger)) {
        // Update flags based on new checkbox state
        flags = isTrigger ? (flags | 0x1) : (flags & ~0x1);
        data["Flags"] = flags;
    }

    EditorUI::RenderVector2DRow("Offset##Circle", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Radius##Circle", "px", data, "Radius", 1.0f);
    EditorUI::RenderIntProperty("Layer Mask##Circle", data, "LayerMask");
    EditorUI::EndPropertySection();
}

// Render a BoxCollider2D including offset, half extents, and rotation.
// Supports a trigger flag and layer mask editing.
void ComponentUI::RenderBoxCollider2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Half Extents", "Rotation", "Layer Mask" });

    // Extract current flag value
    int flags = data.value("Flags", 0);
    bool isTrigger = (flags & 0x1) != 0;

    // Render checkbox and get if it changed
    if (EditorUI::RenderCheckboxPropertyReturn("Is Trigger##Box", isTrigger)) {
        // Update flags based on new checkbox state
        flags = isTrigger ? (flags | 0x1) : (flags & ~0x1);
        data["Flags"] = flags;
    }

    EditorUI::RenderVector2DRow("Offset##Box", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Half Extents##Box", data["HalfExtents"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Rotation##Box", "degrees", data, "Rotation", 1.0f);
    EditorUI::RenderIntProperty("Layer Mask##Box", data, "LayerMask");

    EditorUI::EndPropertySection();
}

// Render a simple 2D circle shape for debug drawing.
// Includes thickness and filled options for visualization.
void ComponentUI::RenderShapeCircle2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Radius", "Offset", "Color", "Thickness", "Filled" });

    EditorUI::RenderFloatRow("Radius##ShapeCircle2D", "px", data, "Radius", 1.0f);
    EditorUI::RenderVector2DRow("Offset##ShapeCircle2D", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderColorProperty("Color##ShapeCircle2D", data["Color"]);
    EditorUI::RenderFloatRow("Thickness##ShapeCircle2D", "px", data, "Thickness", 0.1f);
    EditorUI::RenderCheckboxProperty("Filled##ShapeCircle2D", data, "Filled");

    EditorUI::EndPropertySection();
}

// Render a simple 2D box shape for debug drawing.
// Matches the property layout used in collider components.
void ComponentUI::RenderShapeBox2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Half Extents", "Offset", "Color", "Thickness", "Filled" });

    EditorUI::RenderVector2DRow("Half Extents##ShapeBox2D", data["HalfExtents"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Offset##ShapeBox2D", data["Offset"], "X", "Y", 1.0f);
    EditorUI::RenderColorProperty("Color##ShapeBox2D", data["Color"]);
    EditorUI::RenderFloatRow("Thickness##ShapeBox2D", "px", data, "Thickness", 0.1f);
    EditorUI::RenderCheckboxProperty("Filled##ShapeBox2D", data, "Filled");

    EditorUI::EndPropertySection();
}

// Render a 2D line segment with endpoints and thickness.
// Uses color editing helper for consistent RGBA handling.
void ComponentUI::RenderShapeLine2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Point A", "Point B", "Thickness", "Color" });

    EditorUI::RenderVector2DRow("Point A##ShapeLine2D", data["A"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Point B##ShapeLine2D", data["B"], "X", "Y", 1.0f);
    EditorUI::RenderFloatRow("Thickness##ShapeLine2D", "px", data, "Thickness", 0.1f);
    EditorUI::RenderColorProperty("Color##ShapeLine2D", data["Color"]);

    EditorUI::EndPropertySection();
}

// Render the Camera3D component with mode and clip settings.
// Toggles between perspective FOV or ortho size controls.
void ComponentUI::RenderCamera3D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Mode", "Near Plane", "Far Plane", "Aspect Ratio", "FOV", "Ortho Size" });

    EditorUI::RenderCheckboxRow("Mode", data, "Active", "Active", "UsePerspective", "Perspective");

    bool usePerspective = data.value("UsePerspective", false);
    if (usePerspective) {
        EditorUI::RenderFloatRow("FOV", "deg", data, "FOV", 0.5f);
    }
    else {
        EditorUI::RenderFloatRow("Ortho Size", "units", data, "OrthoSize", 0.5f);
    }

    EditorUI::RenderFloatRow("Near Plane", "", data, "NearPlane", 0.01f);
    EditorUI::RenderFloatRow("Far Plane", "", data, "FarPlane", 0.1f);
    EditorUI::RenderFloatRow("Aspect Ratio", "w/h", data, "AspectRatio", 0.01f);

    EditorUI::EndPropertySection();
}
