/* Start Header *****************************************************************/
/*!
\file   ComponentPropertyEditor.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025

\brief
Implements the ComponentUI class which draws the editor UI for every component type.

This file contains the detailed ImGui rendering code for all supported components,
including transforms, sprites, physics bodies, colliders, shapes and camera data.
Each function edits JSON-backed component properties so both runtime entities and
prefab assets use the same UI path.
*/
/* End Header *******************************************************************/

#include "ComponentPropertyEditor.h"
#include "ComponentWidgets.h"
#include "core/Logger.h"
#include <imgui.h>
#include <cmath>
#include "EditorStyle.h"
#include <filesystem>
#include "ecs/Components.h"
#include "serialization/EntitySerializer.h"
#include "services/ResourceManager.h"
#include <algorithm>
#include "AudioAssetLibrary.h"

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------

// Sets the fonts used for all component property UIs
// We keep these pointers so every widget uses a consistent visual style
void ComponentUI::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

// -----------------------------------------------------------------------------
// Component Rendering
// -----------------------------------------------------------------------------

// Renders the Name component properties
void ComponentUI::RenderName(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Name" });

    // Get current name value
    std::string name = data.value("Value", std::string("Entity"));
    char buffer[256];
    strncpy_s(buffer, name.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Render text input for name
    ImGui::Text("Name");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("##Name", buffer, sizeof(buffer))) {
        data["Value"] = std::string(buffer);
    }

    EditorUI::EndPropertySection();
}

// Renders the Active component properties
void ComponentUI::RenderActive(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Active" });
    EditorUI::RenderCheckboxProperty("Enabled", data, "Enabled");
    EditorUI::EndPropertySection();
}

// Renders the TagMask component properties
void ComponentUI::RenderTagMask(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Tag Mask" });

    // Tag mask is a bitfield stored as integer
    int mask = data.value("Mask", 0);

    ImGui::Text("Mask");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputInt("##Mask", &mask)) {
        data["Mask"] = mask;
    }

    EditorUI::EndPropertySection();
}

// Renders the Lifetime component properties
void ComponentUI::RenderLifetime(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Lifetime" });
    EditorUI::RenderFloatRow("Time##Lifetime", "s", data, "Time", 0.1f);
    EditorUI::EndPropertySection();
}

// Renders the LocalTransform component properties
void ComponentUI::RenderLocalTransform(nlohmann::json& data) {
    // Start a grouped section so all three rows share aligned labels
    EditorUI::BeginPropertySection({ "Local Rotation", "Local Position", "Local Scale" });

    // Draw rotation as Euler angles for the editor UI (degrees).
    // Internal storage remains a quaternion (X,Y,Z,W). We convert back-and-forth.
    {
        // Ensure rotation object exists
        if (!data.contains("Rotation") || !data["Rotation"].is_object())
            data["Rotation"] = nlohmann::json::object();

        // Read quaternion components (default identity)
        float qx = data["Rotation"].value("X", 0.0f);
        float qy = data["Rotation"].value("Y", 0.0f);
        float qz = data["Rotation"].value("Z", 0.0f);
        float qw = data["Rotation"].value("W", 1.0f);

        // Convert quaternion -> rotation matrix elements
        // Using standard conversion: matrix elements m_ij
        float m00 = 1.0f - 2.0f * (qy * qy + qz * qz);
        float m01 = 2.0f * (qx * qy - qz * qw);
        float m02 = 2.0f * (qx * qz + qy * qw);
        float m10 = 2.0f * (qx * qy + qz * qw);
        float m11 = 1.0f - 2.0f * (qx * qx + qz * qz);
        float m12 = 2.0f * (qy * qz - qx * qw);
        float m20 = 2.0f * (qx * qz - qy * qw);
        float m21 = 2.0f * (qy * qz + qx * qw);
        float m22 = 1.0f - 2.0f * (qx * qx + qy * qy);

        // Decompose assuming rotation order: apply X (pitch), then Y (yaw), then Z (roll)
        // This matches Quaternion::FromEulerRad(pitch, yaw, roll) used elsewhere.
        auto clampf = [](float v, float lo, float hi) {
            if (v < lo) return lo;
            if (v > hi) return hi;
            return v;
        };

        float yawRad = std::asin(clampf(-m20, -1.0f, 1.0f));
        float cosy = std::cos(yawRad);
        float pitchRad = 0.0f;
        float rollRad = 0.0f;
        const float EPS = 1e-6f;
        if (std::fabs(cosy) > EPS) {
            pitchRad = std::atan2(m21, m22);
            rollRad  = std::atan2(m10, m00);
        }
        else {
            // Gimbal lock: set pitch to 0, compute roll from alternative terms
            pitchRad = 0.0f;
            rollRad = std::atan2(-m01, m11);
        }

        // Convert to degrees for display
        const float RAD_TO_DEG = 180.0f / 3.14159265358979323846f;
        float degX = pitchRad * RAD_TO_DEG;
        float degY = yawRad   * RAD_TO_DEG;
        float degZ = rollRad  * RAD_TO_DEG;

        // Draw three degree fields aligned like Vector3DRow
        ImGui::Text("%s", "Local Rotation");

        const float fieldWidth = 90.0f;
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;
        const float valueStart = EditorUI::GetContentStartX();

        // X (Pitch)
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStart);
        ImGui::Text("X");
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStart + axisLabelWidth + 6.0f);
        ImGui::SetNextItemWidth(fieldWidth);
        bool changed = false;
        if (ImGui::DragFloat("##LocalRotX", &degX, 0.1f, -360.0f, 360.0f, "%.2f")) changed = true;

        // Y (Yaw)
        float yStartX = valueStart + (axisLabelWidth + 6.0f + fieldWidth + 20.0f);
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX);
        ImGui::Text("Y");
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX + axisLabelWidth + 6.0f);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat("##LocalRotY", &degY, 0.1f, -360.0f, 360.0f, "%.2f")) changed = true;

        // Z (Roll)
        float zStartX = valueStart + 2 * (axisLabelWidth + 6.0f + fieldWidth + 20.0f);
        ImGui::SameLine();
        ImGui::SetCursorPosX(zStartX);
        ImGui::Text("Z");
        ImGui::SameLine();
        ImGui::SetCursorPosX(zStartX + axisLabelWidth + 6.0f);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat("##LocalRotZ", &degZ, 0.1f, -360.0f, 360.0f, "%.2f")) changed = true;

        // If user changed Euler degrees, convert back to quaternion (radians) and write into JSON
        if (changed) {
            const float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
            float p = degX * DEG_TO_RAD;
            float y = degY * DEG_TO_RAD;
            float r = degZ * DEG_TO_RAD;
            Quaternion nq = Quaternion::FromEulerRad(p, y, r);
            data["Rotation"]["X"] = nq.X;
            data["Rotation"]["Y"] = nq.Y;
            data["Rotation"]["Z"] = nq.Z;
            data["Rotation"]["W"] = nq.W;
        }
    }

    // Draw position as a 3D vector with X Y Z fields
    EditorUI::RenderVector3DRow("Local Position", data["Position"], "X", "Y", "Z", 1.0f);

    // Draw rotation as a quaternion with X Y Z W components
    EditorUI::RenderQuaternionRow("Local Rotation", data["Rotation"], "X", "Y", "Z", "W", 0.1f);

    // Draw scale as a 3D vector with X Y Z fields
    // Smaller dragSpeed so scaling changes are more precise
    EditorUI::RenderVector3DRow("Local Scale", data["Scale"], "X", "Y", "Z", 0.01f);

    // Close the grouped section and restore layout state
    EditorUI::EndPropertySection();
}

// Renders the SpriteRenderer2D component properties
void ComponentUI::RenderSpriteRenderer2D(nlohmann::json& data) {
    // RELOAD TEXTURE FROM PATH ON FIRST RENDER
    // Build a human readable summary of the current texture
    std::string texPath = data.value("TexturePath", "");
    std::string valueText;
    if (!texPath.empty()) {
        // Show only the file name instead of the full path for readability
        valueText = std::filesystem::path(texPath).filename().string();

        // Reload texture from path if TextureId is 0 or invalid
        // This happens when loading a scene - TexturePath is saved but TextureId is not persistent
        uint32_t currentId = data.value("TextureId", 0u);
        if (currentId == 0) {
            auto tex = RM.Get<Texture>(texPath);
            if (tex) {
                data["TextureId"] = static_cast<uint32_t>(tex->ID());
                data["Width"] = tex->Width();
                data["Height"] = tex->Height();
                LOG_DEBUG("Reloaded texture from path: " << texPath << ", id=" << tex->ID());
            }
            else {
                LOG_WARNING("Failed to reload texture from path: " << texPath);
            }
        }
    }
    else {
        valueText = "None (drag texture here)";
    }

    // Group all sprite related rows under one aligned section
    EditorUI::BeginPropertySection({ "Sprite", "Color", "Tiling", "Offset" });

    // Show the sprite information in a read only row
    EditorUI::RenderStaticValueRow("Sprite", valueText, texPath.empty());

    // Tracks whether a valid texture was dropped this frame
    bool dropped = false;

    // Allow users to drag a texture asset from the asset browser into this control
    if (ImGui::BeginDragDropTarget()) {
        // Accept payloads tagged as ASSET_PATH which contain a file path string
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            // Payload data is a char buffer containing the file path
            std::string droppedPath = static_cast<const char*>(payLoad->Data);

            // Extract file extension and normalise to lowercase for comparison
            auto ext = std::filesystem::path(droppedPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            // Only accept common image formats as sprite textures
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                // Load texture through the resource manager using the dropped path
                auto tex = RM.Get<Texture>(droppedPath);
                if (tex) {
                    // Store texture ID and path in JSON so renderer can use them
                    data["TextureId"] = static_cast<uint32_t>(tex->ID());
                    data["TexturePath"] = droppedPath;

                    // Cache width and height for UV or layout calculations
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
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
            const char* dataBuf = static_cast<const char*>(payLoad->Data);
            const char* end = dataBuf + payLoad->DataSize;
            while (dataBuf < end) {
                std::string path(dataBuf);
                dataBuf += path.size() + 1;
                if (path.empty()) continue;
                auto ext = std::filesystem::path(path).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") continue;
                auto tex = RM.Get<Texture>(path);
                if (tex) {
                    data["TextureId"] = static_cast<uint32_t>(tex->ID());
                    data["TexturePath"] = path;
                    data["Width"] = tex->Width();
                    data["Height"] = tex->Height();
                    dropped = true;
                    LOG_INFO("Dropped texture: " << path << ", id=" << tex->ID());
                }
                else {
                    LOG_ERROR("Failed to load dropped texture: " << path);
                }
                break;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // If a valid texture was dropped, show a small success message inline
    if (dropped) {
        ImGui::SameLine();
        ImGui::TextColored(EditorStyle::SuccessText, "Texture updated");
    }

    // Color tint applied on top of the sprite
    EditorUI::RenderColorProperty("Color##Sprite", data["Color"]);

    // UV tiling controls how many times the texture repeats over the shape
    EditorUI::RenderVector2DRow("Tiling##Sprite", data["Tiling"], "X", "Y", 0.1f);

    // UV offset shifts the texture sampling across the sprite
    EditorUI::RenderVector2DRow("Offset##Sprite", data["Offset"], "X", "Y", 0.1f);
    EditorUI::EndPropertySection();
}

// Renders the Rigidbody2D physics component properties
void ComponentUI::RenderRigidbody2D(nlohmann::json& data) {
    // Group all rigidbody fields so labels line up and scrolling feels consistent
    EditorUI::BeginPropertySection({ "Mass", "Inverse Mass", "Linear Damping", "Angular Damping",
        "Gravity Scale", "Flags" });

    // Mass in kilograms
    EditorUI::RenderFloatRow("Mass", "kg", data, "Mass", 0.1f);

    // Inverse mass is 1 / mass 
    EditorUI::RenderFloatRow("Inverse Mass", "1/kg", data, "InverseMass", 0.1f);

    // Linear damping slows translational motion over time
    EditorUI::RenderFloatRow("Linear Damping", "", data, "LinearDamping", 0.01f);

    // Angular damping slows rotational motion over time
    EditorUI::RenderFloatRow("Angular Damping", "", data, "AngularDamping", 0.01f);

    // Gravity scale lets this body feel heavier or lighter than global gravity
    EditorUI::RenderFloatRow("Gravity Scale", "", data, "GravityScale", 0.1f);

    // Flags hold raw bit values used by the physics engine
    EditorUI::RenderIntProperty("Flags", data, "Flags");
    EditorUI::EndPropertySection();
}

// Renders the LinearVelocity2D component
void ComponentUI::RenderLinearVelocity2D(nlohmann::json& data) {
    // Single row section for velocity vector
    EditorUI::BeginPropertySection({ "Linear Velocity" });

    // data["Value"] stores the X and Y components for velocity
    EditorUI::RenderVector2DRow("Velocity##Linear", data["Value"], "X", "Y", 1.0f);
    EditorUI::EndPropertySection();
}

// Renders the AngularVelocity2D component
void ComponentUI::RenderAngularVelocity2D(nlohmann::json& data) {
    // Single row section for angular velocity
    EditorUI::BeginPropertySection({ "Angular Velocity" });

    // Value represents speed in degrees per second
    EditorUI::RenderFloatRow("Angular Velocity##Angular", "deg/s", data, "Value", 0.5f);
    EditorUI::EndPropertySection();
}

// Renders the CircleCollider2D component
void ComponentUI::RenderCircleCollider2D(nlohmann::json& data) {
    // Group rows for trigger, offset, radius and layer mask together
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Radius", "Layer Mask" });

    // Flags field stores collider settings as bits packed in an integer
    int flags = data.value("Flags", 0);

    // Bit 0 controls whether this collider is a trigger (no physical collision)
    bool isTrigger = (flags & 0x1) != 0;

    // Render a checkbox which both displays and edits the trigger flag
    // Function returns true only when the user changes the checkbox state
    if (EditorUI::RenderCheckboxPropertyReturn("Is Trigger##Circle", isTrigger)) {
        // If enabled, set bit 0 to 1 by ORing with 0x1
        // If disabled, clear bit 0 by ANDing with the inverse of 0x1
        data["Flags"] = isTrigger ? (flags | 0x1) : (flags & ~0x1);
    }

    // Offset moves the collider shape relative to the entity origin
    EditorUI::RenderVector2DRow("Offset##Circle", data["Offset"], "X", "Y", 1.0f);

    // Radius defines how large the circle collider is
    EditorUI::RenderFloatRow("Radius##Circle", "px", data, "Radius", 1.0f);

    // Layer mask decides which other layers this collider can interact with
    EditorUI::RenderIntProperty("Layer Mask##Circle", data, "LayerMask");
    EditorUI::EndPropertySection();
}

// Renders the BoxCollider2D component
void ComponentUI::RenderBoxCollider2D(nlohmann::json& data) {
    // Group rows for trigger, offset, size, rotation and layer mask
    EditorUI::BeginPropertySection({ "Is Trigger", "Offset", "Half Extents", "Rotation",
        "Layer Mask" });

    // Use the same Flags bit layout as the circle collider
    int flags = data.value("Flags", 0);
    bool isTrigger = (flags & 0x1) != 0;

    // Toggle bit 0 whenever the checkbox is changed
    if (EditorUI::RenderCheckboxPropertyReturn("Is Trigger##Box", isTrigger)) {
        data["Flags"] = isTrigger ? (flags | 0x1) : (flags & ~0x1);
    }

    // Offset moves the box shape relative to the entity origin
    EditorUI::RenderVector2DRow("Offset##Box", data["Offset"], "X", "Y", 1.0f);

    // Half extents describe half the width and half the height of the box
    EditorUI::RenderVector2DRow("Half Extents##Box", data["HalfExtents"], "X", "Y", 1.0f);

    // Rotation in degrees rotates the collider around its center
    EditorUI::RenderFloatRow("Rotation##Box", "degrees", data, "Rotation", 1.0f);

    // Layer mask selects which collision layers the box interacts with
    EditorUI::RenderIntProperty("Layer Mask##Box", data, "LayerMask");
    EditorUI::EndPropertySection();
}

// Renders the ShapeCircle2D debug draw component
void ComponentUI::RenderShapeCircle2D(nlohmann::json& data) {
    // Group radius, offset, color, thickness and filled flag together
    EditorUI::BeginPropertySection({ "Radius", "Offset", "Color", "Thickness", "Filled" });

    // Radius defines how large the rendered circle is
    EditorUI::RenderFloatRow("Radius##ShapeCircle2D", "px", data, "Radius", 1.0f);

    // Offset moves the rendered circle relative to the entity origin
    EditorUI::RenderVector2DRow("Offset##ShapeCircle2D", data["Offset"], "X", "Y", 1.0f);

    // Color controls the tint of the line or fill
    EditorUI::RenderColorProperty("Color##ShapeCircle2D", data["Color"]);

    // Thickness affects how thick the outline is when not filled
    EditorUI::RenderFloatRow("Thickness##ShapeCircle2D", "px", data, "Thickness", 0.1f);

    // Filled decides between a solid circle and just an outline
    EditorUI::RenderCheckboxProperty("Filled##ShapeCircle2D", data, "Filled");
    EditorUI::EndPropertySection();
}

// Renders the ShapeBox2D debug draw component
void ComponentUI::RenderShapeBox2D(nlohmann::json& data) {
    // Group size, offset, color, thickness and filled flag together
    EditorUI::BeginPropertySection({ "Half Extents", "Offset", "Color", "Thickness", "Filled" });

    // Half extents describe half the width and half the height of the rendered box
    EditorUI::RenderVector2DRow("Half Extents##ShapeBox2D", data["HalfExtents"], "X", "Y", 1.0f);

    // Offset moves the box shape relative to the entity origin
    EditorUI::RenderVector2DRow("Offset##ShapeBox2D", data["Offset"], "X", "Y", 1.0f);

    // Color controls the outline or fill tint
    EditorUI::RenderColorProperty("Color##ShapeBox2D", data["Color"]);

    // Thickness sets the width of the outline lines
    EditorUI::RenderFloatRow("Thickness##ShapeBox2D", "px", data, "Thickness", 0.1f);

    // Filled toggles between a solid rectangle and a wireframe rectangle
    EditorUI::RenderCheckboxProperty("Filled##ShapeBox2D", data, "Filled");
    EditorUI::EndPropertySection();
}

// Renders the ShapeLine2D debug draw component
void ComponentUI::RenderShapeLine2D(nlohmann::json& data) {
    // Group both endpoints, thickness and color in a single section
    EditorUI::BeginPropertySection({ "Point A", "Point B", "Thickness", "Color" });

    // Point A is the starting coordinate of the line
    EditorUI::RenderVector2DRow("Point A##ShapeLine2D", data["A"], "X", "Y", 1.0f);

    // Point B is the ending coordinate of the line
    EditorUI::RenderVector2DRow("Point B##ShapeLine2D", data["B"], "X", "Y", 1.0f);

    // Thickness controls how thick the line is drawn
    EditorUI::RenderFloatRow("Thickness##ShapeLine2D", "px", data, "Thickness", 0.1f);

    // Color sets the tint of the line
    EditorUI::RenderColorProperty("Color##ShapeLine2D", data["Color"]);
    EditorUI::EndPropertySection();
}

// Renders the Camera3D component properties
// User can choose between perspective and orthographic modes
void ComponentUI::RenderCamera3D(nlohmann::json& data) {
    // Group camera mode and projection related settings together
    EditorUI::BeginPropertySection({ "Mode", "Near Plane", "Far Plane", "Aspect Ratio", "FOV",
        "Ortho Size" });

    // Two checkboxes on one row represent properties stored in JSON
    // "Active" indicates if this camera is currently used
    // "UsePerspective" decides which projection mode to use
    EditorUI::RenderCheckboxRow("Mode", data, "Active", "Active", "UsePerspective", "Perspective");

    // If perspective mode is enabled, show FOV control
    if (data.value("UsePerspective", false)) {
        // FOV measured in degrees controls how wide the camera view is
        EditorUI::RenderFloatRow("FOV", "deg", data, "FOV", 0.5f);
    }
    else {
        // Otherwise for orthographic mode show the orthographic size
        // Ortho size controls how much world space height the camera sees
        EditorUI::RenderFloatRow("Ortho Size", "units", data, "OrthoSize", 0.5f);
    }

    // Near plane is the closest distance that gets rendered
    EditorUI::RenderFloatRow("Near Plane", "", data, "NearPlane", 0.01f);

    // Far plane is the furthest distance that gets rendered
    EditorUI::RenderFloatRow("Far Plane", "", data, "FarPlane", 0.1f);

    // Aspect ratio is the ratio of width to height used for projection
    EditorUI::RenderFloatRow("Aspect Ratio", "w/h", data, "AspectRatio", 0.01f);
    EditorUI::EndPropertySection();
}

// Renders the Acceleration2D component properties
// Shows the 2D acceleration vector that affects physics bodies
void ComponentUI::RenderAcceleration2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Acceleration" });

    // X and Y components of the acceleration vector
    EditorUI::RenderVector2DRow("Acceleration", data["Value"], "X", "Y", 0.1f);
    EditorUI::EndPropertySection();
}

// Renders the PhysicsMaterial2D component properties
// Controls friction, bounciness and position correction for colliders
void ComponentUI::RenderPhysicsMaterial2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Friction", "Restitution", "Position Correct" });

    // Friction coefficient (0 = frictionless, 1 = very sticky)
    EditorUI::RenderFloatRow("Friction", "", data, "Friction", 0.01f);

    // Restitution (bounciness) - 0 = no bounce, 1 = perfect bounce
    EditorUI::RenderFloatRow("Restitution", "", data, "Restitution", 0.01f);

    // How much of penetration to correct each frame (stability vs accuracy)
    EditorUI::RenderFloatRow("Position Correct", "%", data, "PositionCorrectPercent", 0.01f);
    EditorUI::EndPropertySection();
}


// Renders the SpriteSheetAnimation2D component properties
// Controls animated sprite playback from sprite sheets
void ComponentUI::RenderSpriteSheetAnimation2D(nlohmann::json& data) {
    // RELOAD TEXTURE FROM PATH ON FIRST RENDER
    // Build a human readable summary of the current texture
    std::string texPath = data.value("TexturePath", "");
    std::string valueText;
    if (!texPath.empty()) {
        // Show only the file name instead of the full path for readability
        valueText = std::filesystem::path(texPath).filename().string();

        // Reload texture from path if TextureId is 0 or invalid
        // This happens when loading a scene - TexturePath is saved but TextureId is not persistent
        uint32_t currentId = data.value("TextureId", 0u);
        if (currentId == 0) {
            auto tex = RM.Get<Texture>(texPath);
            if (tex) {
                data["TextureId"] = static_cast<uint32_t>(tex->ID());
                data["SheetWidth"] = tex->Width();
                data["SheetHeight"] = tex->Height();
                LOG_DEBUG("Reloaded sprite sheet from path: " << texPath << ", id=" << tex->ID());
            }
            else {
                LOG_WARNING("Failed to reload sprite sheet from path: " << texPath);
            }
        }
    }
    else {
        valueText = "None (drag sprite sheet here)";
    }


    // Group all sprite sheet related rows under one aligned section
    EditorUI::BeginPropertySection({ "Sprite Sheet", "Frame Width", "Frame Height", "Sheet Width", "Sheet Height", "Start Frame", "Frame Count", "FPS", "Loop", "Playing" });

    // Show the sprite sheet information in a read only row
    EditorUI::RenderStaticValueRow("Sprite Sheet", valueText, texPath.empty());

    // Tracks whether a valid texture was dropped this frame
    bool dropped = false;

    // Allow users to drag a texture asset from the asset browser into this control
    if (ImGui::BeginDragDropTarget()) {
        // Accept payloads tagged as ASSET_PATH which contain a file path string
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            // Payload data is a char buffer containing the file path
            std::string droppedPath = static_cast<const char*>(payLoad->Data);

            // Extract file extension and normalise to lowercase for comparison
            auto ext = std::filesystem::path(droppedPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            // Only accept common image formats as sprite sheet textures
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                // Load texture through the resource manager using the dropped path
                auto tex = RM.Get<Texture>(droppedPath);
                if (tex) {
                    // Store texture ID and path in JSON so renderer can use them
                    data["TextureId"] = static_cast<uint32_t>(tex->ID());
                    data["TexturePath"] = droppedPath;

                    // Cache sheet dimensions from the texture
                    data["SheetWidth"] = tex->Width();
                    data["SheetHeight"] = tex->Height();

                    dropped = true;
                    LOG_INFO("Dropped sprite sheet: " << droppedPath << ", id=" << tex->ID());
                }
                else {
                    LOG_ERROR("Failed to load dropped sprite sheet: " << droppedPath);
                }
            }
        }
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
            const char* dataBuf = static_cast<const char*>(payLoad->Data);
            const char* end = dataBuf + payLoad->DataSize;
            while (dataBuf < end) {
                std::string path(dataBuf);
                dataBuf += path.size() + 1;
                if (path.empty()) continue;
                auto ext = std::filesystem::path(path).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") continue;
                auto tex = RM.Get<Texture>(path);
                if (tex) {
                    data["TextureId"] = static_cast<uint32_t>(tex->ID());
                    data["TexturePath"] = path;
                    data["SheetWidth"] = tex->Width();
                    data["SheetHeight"] = tex->Height();
                    dropped = true;
                    LOG_INFO("Dropped sprite sheet: " << path << ", id=" << tex->ID());
                }
                else {
                    LOG_ERROR("Failed to load dropped sprite sheet: " << path);
                }
                break;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Individual frame dimensions
    EditorUI::RenderIntProperty("Frame Width", data, "FrameWidth");
    EditorUI::RenderIntProperty("Frame Height", data, "FrameHeight");

    // Total sprite sheet dimensions (auto-filled when texture is dropped)
    EditorUI::RenderIntProperty("Sheet Width", data, "SheetWidth");
    EditorUI::RenderIntProperty("Sheet Height", data, "SheetHeight");

    // Which frame to start the animation from
    EditorUI::RenderIntProperty("Start Frame", data, "StartFrame");

    // How many frames in the animation sequence
    EditorUI::RenderIntProperty("Frame Count", data, "FrameCount");

    // Animation speed in frames per second
    EditorUI::RenderFloatRow("FPS", "", data, "FramesPerSecond", 0.5f);

    // Playback controls
    EditorUI::RenderCheckboxProperty("Loop", data, "Loop");
    EditorUI::RenderCheckboxProperty("Playing", data, "Playing");

    EditorUI::EndPropertySection();
}

// Renders the UIclickable component properties
void ComponentUI::RenderUIClickable(nlohmann::json& data) {
    ImGui::PushFont(m_mainFont);

    // Enabled toggle
    bool enabled = data.value("Enabled", true);
    if (ImGui::Checkbox("Enabled", &enabled)) {
        data["Enabled"] = enabled;
    }

    // Is Hovered (read-only display)
    bool isHovered = data.value("IsHovered", false);
    ImGui::BeginDisabled();
    ImGui::Checkbox("Is Hovered", &isHovered);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Read-only: Updated by BoundaryCheckSystem");
    }

    // Was Clicked (read-only display)
    bool wasClicked = data.value("WasClicked", false);
    ImGui::BeginDisabled();
    ImGui::Checkbox("Was Clicked", &wasClicked);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Read-only: True for one frame after click");
    }

    // Click Action ID
    uint32_t actionId = data.value("ClickActionID", 1.0f);
    int actionIdInt = static_cast<int>(actionId);
    if (ImGui::InputInt("Action ID", &actionIdInt)) {
        if (actionIdInt >= 0) {
            data["ClickActionID"] = static_cast<uint32_t>(actionIdInt);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Unique identifier for this UI element's action");
    }

    ImGui::PopFont();
}

// Renders the ZIndex2D component properties
// Controls the rendering order in 2D (higher values render on top)
void ComponentUI::RenderZIndex2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Z-Order" });

    // Integer Z-order value (can be negative)
    EditorUI::RenderIntProperty("Z-Order", data, "ZOrder");
    EditorUI::EndPropertySection();
}

// Renders the Light2D component properties
void ComponentUI::RenderLight2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Light Type", "Position", "Direction", "Color", "Intensity", "Range", "Casts Shadows" });

    // Light type selection (Directional = 0, Point = 1)
    int lightType = data.value("LightType", 0);
    const char* lightTypes[] = { "Directional", "Point" };

    ImGui::Text("Light Type");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo("##LightType", &lightType, lightTypes, 2)) {
        data["LightType"] = lightType;
    }

    // Position (used for Point lights)
    EditorUI::RenderVector3DRow("Position##Light2D", data["Position"], "X", "Y", "Z", 0.1f);

    // Direction (used for Directional lights)
    EditorUI::RenderVector3DRow("Direction##Light2D", data["Direction"], "X", "Y", "Z", 0.1f);

    // Color
    if (!data.contains("Color")) {
        data["Color"] = nlohmann::json{ {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    }
    EditorUI::RenderColorProperty("Color##Light2D", data["Color"]);

    // Intensity
    EditorUI::RenderFloatRow("Intensity##Light2D", "", data, "Intensity", 0.1f);

    // Range (for Point lights)
    EditorUI::RenderFloatRow("Range##Light2D", "units", data, "Range", 0.5f);

    // Casts Shadows
    EditorUI::RenderCheckboxProperty("Casts Shadows##Light2D", data, "CastsShadows");

    EditorUI::EndPropertySection();
}

// Renders the Text component properties
void ComponentUI::RenderText(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Content", "Font Path", "Pixel Size", "Color", "Anchor" });

    // Text content
    std::string content = data.value("Content", std::string("Text"));
    char contentBuffer[256];
    strncpy_s(contentBuffer, content.c_str(), sizeof(contentBuffer) - 1);
    contentBuffer[sizeof(contentBuffer) - 1] = '\0';

    ImGui::Text("Content");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::InputText("##Content", contentBuffer, sizeof(contentBuffer))) {
        data["Content"] = std::string(contentBuffer);
    }

    // Font path
    std::string fontPath = data.value("FontPath", std::string("assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf"));
    char fontBuffer[128];
    strncpy_s(fontBuffer, fontPath.c_str(), sizeof(fontBuffer) - 1);
    fontBuffer[sizeof(fontBuffer) - 1] = '\0';

    ImGui::Text("Font Path");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::InputText("##FontPath", fontBuffer, sizeof(fontBuffer))) {
        data["FontPath"] = std::string(fontBuffer);
    }

    // Pixel size
    EditorUI::RenderFloatRow("Pixel Size##Text", "px", data, "PixelSize", 1.0f);

    // Color
    if (!data.contains("Color")) {
        data["Color"] = nlohmann::json{ {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    }
    EditorUI::RenderColorProperty("Color##Text", data["Color"]);

    // Anchor (enum: Absolute, TopLeft, TopRight, BottomLeft, BottomRight, Center)
    int anchor = data.value("Anchor", 0);
    const char* anchors[] = { "Absolute", "Top Left", "Top Right", "Bottom Left", "Bottom Right", "Center" };

    ImGui::Text("Anchor");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo("##Anchor", &anchor, anchors, 6)) {
        data["Anchor"] = anchor;
    }

    EditorUI::EndPropertySection();
}

// Renders the AnimationState2D component properties
void ComponentUI::RenderAnimationState2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Current Frame", "Time Accumulator", "Finished" });

    // Current frame (integer)
    EditorUI::RenderIntProperty("Current Frame##AnimState", data, "CurrentFrame");

    // Time accumulator (float)
    EditorUI::RenderFloatRow("Time Accumulator##AnimState", "s", data, "TimeAccumulator", 0.01f);

    // Finished (boolean)
    EditorUI::RenderCheckboxProperty("Finished##AnimState", data, "Finished");

    EditorUI::EndPropertySection();
}

// Static variab/flags
static bool s_showUnsupportedPopup = false;
static std::string s_unsupportedPath;


// Renders the AudioSource component Properties
void ComponentUI::RenderAudioSource(nlohmann::json& data)
{
    // Ensure keys exist with defaults
    if (!data.contains("CueId"))       data["CueId"] = 0;
    if (!data.contains("Volume"))      data["Volume"] = 1.0f;
    if (!data.contains("Pitch"))       data["Pitch"] = 1.0f;
    if (!data.contains("Loop"))        data["Loop"] = false;
    if (!data.contains("PlayOnStart")) data["PlayOnStart"] = false;
    if (!data.contains("Spatial3D"))   data["Spatial3D"] = false;

    // SINGLE BeginPropertySection call with ALL field names for proper alignment
    EditorUI::BeginPropertySection({ "Audio Clip", "Volume", "Pitch", "Loop", "Play On Start", "Spatial 3D" });

    uint32_t cueId = data.value("CueId", 0u);
    auto& lib = AudioAssetLibrary::Get();
    const auto& clips = lib.GetAllClips();

    const AudioAssetLibrary::ClipInfo* selectedClip = nullptr;
    for (const auto& clip : clips) {
        if (clip.id == cueId) {
            selectedClip = &clip;
            break;
        }
    }

    std::string currentLabel;
    if (!selectedClip)
        currentLabel = "None (Audio Clip)";
    else
        currentLabel = selectedClip->name;

    // Audio Clip combo box - aligned properly
    ImGui::Text("%s", "Audio Clip");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(90.0f);

    if (ImGui::BeginCombo("##AudioClipCombo", currentLabel.c_str())) {
        // "None" option
        bool isNone = (cueId == 0);
        if (ImGui::Selectable("None (Audio Clip)", isNone)) {
            data["CueId"] = 0;
            cueId = 0;
        }
        if (isNone)
            ImGui::SetItemDefaultFocus();

        // One option per known clip
        for (const auto& clip : clips) {
            bool isSelected = (clip.id == cueId);
            if (ImGui::Selectable(clip.name.c_str(), isSelected)) {
                data["CueId"] = clip.id;
                cueId = clip.id;
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    // Drag-drop support
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* droppedPath = static_cast<const char*>(payload->Data);
            std::filesystem::path path(droppedPath);

            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            bool supported = (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac");

            if (supported) {
                const auto& clipInfo = lib.Register(path.string());
                data["CueId"] = clipInfo.id;
            }
            else {
                s_showUnsupportedPopup = true;
                s_unsupportedPath = path.string();
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
            const char* dataBuf = static_cast<const char*>(payload->Data);
            const char* end = dataBuf + payload->DataSize;
            while (dataBuf < end) {
                std::string pathStr(dataBuf);
                dataBuf += pathStr.size() + 1;
                if (pathStr.empty()) continue;
                std::filesystem::path p(pathStr);
                std::string ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                bool supported = (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac");
                if (!supported) continue;
                const auto& clipInfo = lib.Register(p.string());
                data["CueId"] = clipInfo.id;
                break;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Volume + Pitch sliders using EditorUI helpers
    EditorUI::RenderFloatRow("Volume", "", data, "Volume", 0.05f);
    EditorUI::RenderFloatRow("Pitch", "", data, "Pitch", 0.05f);

    // Checkboxes
    EditorUI::RenderCheckboxProperty("Loop", data, "Loop");
    EditorUI::RenderCheckboxProperty("Play On Start", data, "PlayOnStart");
    EditorUI::RenderCheckboxProperty("Spatial 3D", data, "Spatial3D");

    // SINGLE EndPropertySection call
    EditorUI::EndPropertySection();

    // Unsupported format popup
    if (s_showUnsupportedPopup) {
        ImGui::OpenPopup("Unsupported Audio Format");
        ImGui::SetNextWindowSize(ImVec2(450, 250), ImGuiCond_Appearing);
    }

    if (ImGui::BeginPopupModal("Unsupported Audio Format", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "The file you tried to assign is not a supported audio format.\n\n"
            "Only .wav, .ogg, .mp3, and .flac files are allowed.\n\n"
        );

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            s_showUnsupportedPopup = false;
            s_unsupportedPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// Renders the Layer component properties
void ComponentUI::RenderLayer2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Layer" });
    EditorUI::RenderIntProperty("Id", data, "Id");
    EditorUI::EndPropertySection();
}

// Renders the ScriptInstance component properties
void ComponentUI::RenderScriptInstance(nlohmann::json& data) {
    // Ensure keys exist with defaults
    if (!data.contains("TypeName")) data["TypeName"] = "";
    if (!data.contains("ScriptPath")) data["ScriptPath"] = "";
    if (!data.contains("Initialized")) data["Initialized"] = false;
    if (!data.contains("ManagedHandle")) data["ManagedHandle"] = 0;
    if (!data.contains("TypeHash")) data["TypeHash"] = 0;

    EditorUI::BeginPropertySection({ "Script Class", "Script Path", "Initialized" });

    // Display script class name (read-only)
    std::string typeName = data.value("TypeName", std::string(""));
    ImGui::Text("Script Class");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::TextColored(EditorStyle::Muted, "%s", typeName.empty() ? "None" : typeName.c_str());
    
    // Display script path (read-only)
    std::string scriptPath = data.value("ScriptPath", std::string(""));
    ImGui::Text("Script Path");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::TextColored(EditorStyle::Muted, "%s", scriptPath.empty() ? "None" : scriptPath.c_str());
    
    // Show initialization status
    bool initialized = data.value("Initialized", false);
    ImGui::Text("Initialized");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::TextColored(
        initialized ? EditorStyle::SuccessText : EditorStyle::WarningText,
        "%s", initialized ? "Yes" : "No"
    );

    EditorUI::EndPropertySection();

    // Modify Script button
    ImGui::Spacing();
    if (!typeName.empty() && !scriptPath.empty()) {
        if (ImGui::Button("Modify Script")) {
            // Use the stored script path
            if (std::filesystem::exists(scriptPath)) {
                // Open the file with the default system editor
#ifdef _WIN32
                std::string command = "start \"\" \"" + scriptPath + "\"";
                system(command.c_str());
#endif
            }
            else {
                LOG_WARNING("Script file not found: " << scriptPath);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Opens script file in default editor");
    }
    else {
        ImGui::TextDisabled("No script attached. Use 'Attach Script' from hierarchy context menu.");
    }
}
