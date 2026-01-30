/* Start Header *****************************************************************/
/*!
\file   ComponentPropertyEditor.cpp
\author Foo Rui Qin (90%)
        Samantha Leong (10%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
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
#include "EditorECSUtils.h"
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
#include "core/Application.h"

namespace {
    ECS::ComponentTypeId GetComponentIdFromHashOrWarn(uint32_t hash, const char* name) {
        const ECS::ComponentTypeId id = ECS::ComponentRegistry::GetComponentIdFromHash(hash);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[ComponentPropertyEditor] Missing component ID for '" << name << "' (hash=0x"
                << std::hex << hash << std::dec << ")");
        }
        return id;
    }
}

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
void ComponentUI::RenderName(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderActive(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    EditorUI::BeginPropertySection({ "Active" });
    EditorUI::RenderCheckboxProperty("Enabled", data, "Enabled");
    EditorUI::EndPropertySection();
}

// Renders the TagMask component properties
void ComponentUI::RenderTagMask(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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

// Renders the LocalTransform component properties
void ComponentUI::RenderLocalTransform(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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

        (void)m12;
        (void)m02;

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
    // EditorUI::RenderQuaternionRow("Local Rotation", data["Rotation"], "X", "Y", "Z", "W", 0.1f);

    // Draw scale as a 3D vector with X Y Z fields
    // Smaller dragSpeed so scaling changes are more precise
    EditorUI::RenderVector3DRow("Local Scale", data["Scale"], "X", "Y", "Z", 0.01f);

    // Close the grouped section and restore layout state
    EditorUI::EndPropertySection();
}

// Renders the SpriteRenderer2D component properties
void ComponentUI::RenderSpriteRenderer2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    // RELOAD TEXTURE FROM PATH ON FIRST RENDER
    // Build a human readable summary of the current texture
    std::string texPath = data.value("TexturePath", "");
    std::string valueText;
    if (!texPath.empty()) {
        // Show only the file name instead of the full path for readability
        valueText = std::filesystem::path(texPath).filename().string();

        // Only reload texture if it's not already loaded (TextureId is 0)
        // The reload happens once when a scene is first loaded, then texture ID persists
        uint32_t currentId = data.value("TextureId", 0u);
        if (currentId == 0) {
            // Only try to load once - if it fails, don't retry on every frame
            if (!data.contains("_TextureLoadAttempted")) {
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
                // Mark that we've attempted to load this texture (even if it failed)
                data["_TextureLoadAttempted"] = true;
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
void ComponentUI::RenderRigidbody2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderLinearVelocity2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    // Single row section for velocity vector
    EditorUI::BeginPropertySection({ "Linear Velocity" });

    // data["Value"] stores the X and Y components for velocity
    EditorUI::RenderVector2DRow("Velocity##Linear", data["Value"], "X", "Y", 1.0f);
    EditorUI::EndPropertySection();
}

// Renders the AngularVelocity2D component
void ComponentUI::RenderAngularVelocity2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    // Single row section for angular velocity
    EditorUI::BeginPropertySection({ "Angular Velocity" });

    // Value represents speed in degrees per second
    EditorUI::RenderFloatRow("Angular Velocity##Angular", "deg/s", data, "Value", 0.5f);
    EditorUI::EndPropertySection();
}

// Renders the CircleCollider2D component
void ComponentUI::RenderCircleCollider2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderBoxCollider2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
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

    // Auto-generates or updates a BoxCollider2D (AABB) for the selected entity based on its sprite size.
    if (ImGui::Button("Generate AABB")) { // Sam
        using namespace ECS::Components;
        
        // Validate world and entity first to avoid dereferencing invalid pointers
        if (!world) {
            LOG_WARNING("Generate AABB called with null world pointer");
        }
        // Ensure the entity is alive before accessing its components
        else if (!world->IsAlive(entity)) {
            LOG_WARNING("Generate AABB called for dead/invalid entity");
        }
        else {
            const ECS::ComponentTypeId spriteId = GetComponentIdFromHashOrWarn(Editor::ECSUtils::FNV1aHash("SpriteRenderer2D"), "SpriteRenderer2D");
            if (spriteId == ECS::NULL_COMPONENT_ID) {
                LOG_WARNING("Generate AABB: SpriteRenderer2D is not registered");
                return;
            }

            // Try to get the sprite component; if missing, bail out gracefully
            auto* sprite = static_cast<SpriteRenderer2D*>(world->GetRawComponentPtr(entity, spriteId));
            if (!sprite) {
                LOG_WARNING("Generate AABB: entity has no SpriteRenderer2D component");
            }
            else {
                // Extract pixel dimensions from the sprite (already loaded by resource manager)
                int pixelWidth = sprite->Width;
                int pixelHeight = sprite->Height;

                // If the entity already has a BoxCollider2D, update it. Otherwise add one.
                const ECS::ComponentTypeId colliderId = GetComponentIdFromHashOrWarn(Editor::ECSUtils::FNV1aHash("BoxCollider2D"), "BoxCollider2D");
                if (colliderId == ECS::NULL_COMPONENT_ID) {
                    LOG_WARNING("Generate AABB: BoxCollider2D is not registered");
                    return;
                }

                auto* col = static_cast<BoxCollider2D*>(world->GetRawComponentPtr(entity, colliderId));
                if (col) {
                    // Update the collider's half-extents based on the sprite size
                    col->HalfExtents = Vector2D{ pixelWidth * 0.5f, pixelHeight * 0.5f };
                }
                else {
                    // No collider yet; create and attach a new BoxCollider2D
                    BoxCollider2D newCol;
                    newCol.HalfExtents = Vector2D{ pixelWidth * 0.5f, pixelHeight * 0.5f };
                    world->AddComponentById(entity, colliderId, &newCol, sizeof(newCol));
                }
            }
        }
    }

    EditorUI::EndPropertySection();
}

// Renders the ShapeCircle2D debug draw component
void ComponentUI::RenderShapeCircle2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderShapeBox2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderShapeLine2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderCamera3D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity; (void)world;
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
void ComponentUI::RenderAcceleration2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    EditorUI::BeginPropertySection({ "Acceleration" });

    // X and Y components of the acceleration vector
    EditorUI::RenderVector2DRow("Acceleration", data["Value"], "X", "Y", 0.1f);
    EditorUI::EndPropertySection();
}

// Renders the PhysicsMaterial2D component properties
// Controls friction, bounciness and position correction for colliders
void ComponentUI::RenderPhysicsMaterial2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderSpriteSheetAnimation2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    // RELOAD TEXTURE FROM PATH ON FIRST RENDER
    // Build a human readable summary of the current texture
    std::string texPath = data.value("TexturePath", "");
    std::string valueText;
    if (!texPath.empty()) {
        // Show only the file name instead of the full path for readability
        valueText = std::filesystem::path(texPath).filename().string();

        // Only reload texture if it's not already loaded (TextureId is 0)
        // The reload happens once when a scene is first loaded, then texture ID persists
        uint32_t currentId = data.value("TextureId", 0u);
        if (currentId == 0) {
            // Only try to load once - if it fails, don't retry on every frame
            if (!data.contains("_TextureLoadAttempted")) {
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
                // Mark that we've attempted to load this texture (even if it failed)
                data["_TextureLoadAttempted"] = true;
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

// Renders the ZIndex2D component properties
// Controls the rendering order in 2D (higher values render on top)
void ComponentUI::RenderZIndex2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    EditorUI::BeginPropertySection({ "Z-Order" });

    // Integer Z-order value (can be negative)
    EditorUI::RenderIntProperty("Z-Order", data, "ZOrder");
    EditorUI::EndPropertySection();
}

// Renders the Light2D component properties
void ComponentUI::RenderLight2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderText(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderAnimationState2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    EditorUI::BeginPropertySection({ "Current Frame", "Time Accumulator", "Finished" });

    // Current frame (integer)
    EditorUI::RenderIntProperty("Current Frame##AnimState", data, "CurrentFrame");

    // Time accumulator (float)
    EditorUI::RenderFloatRow("Time Accumulator##AnimState", "s", data, "TimeAccumulator", 0.01f);

    // Finished (boolean)
    EditorUI::RenderCheckboxProperty("Finished##AnimState", data, "Finished");

    EditorUI::EndPropertySection();
}

// Generic renderer for C# / unknown components. Uses EditorUI helpers where possible
// so the look & feel matches other component UIs.
void ComponentUI::RenderGenericComponent(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity; (void)world;

    if (data.empty()) {
        ImGui::TextDisabled("(C# Component)");
        ImGui::Spacing();
        ImGui::TextWrapped("Component data will be displayed here. Currently, C# component editing requires full field discovery via reflection.");
        return;
    }

    if (!data.is_object()) {
        ImGui::TextDisabled("(Invalid C# Component Data)");
        return;
    }

    // Begin a generic section so fields align with other component rows
    EditorUI::BeginPropertySection({});

    for (auto it = data.begin(); it != data.end(); ++it) {
        const std::string& fieldName = it.key();
        nlohmann::json& fieldValue = it.value();

        if (fieldValue.is_boolean()) {
            EditorUI::RenderCheckboxProperty(fieldName, data, fieldName);
        }
        else if (fieldValue.is_number_integer()) {
            EditorUI::RenderIntProperty(fieldName, data, fieldName);
        }
        else if (fieldValue.is_number_float()) {
            EditorUI::RenderFloatRow(fieldName, "", data, fieldName, 0.1f);
        }
        else if (fieldValue.is_string()) {
            EditorUI::RenderTextProperty(fieldName, data, fieldName);
        }
        else if (fieldValue.is_array()) {
            ImGui::Text("%s (array with %zu elements)", fieldName.c_str(), fieldValue.size());
        }
        else if (fieldValue.is_object()) {
            // Allow expanding nested objects
            if (ImGui::TreeNode(fieldName.c_str())) {
                // Recursively render nested object fields
                RenderGenericComponent(fieldValue, entity, world);
                ImGui::TreePop();
            }
        }
        else {
            ImGui::TextDisabled("%s: (unknown type)", fieldName.c_str());
        }
    }

    EditorUI::EndPropertySection();
}

// Static variab/flags
static bool s_showUnsupportedPopup = false;
static std::string s_unsupportedPath;


// Renders the AudioSource component Properties
void ComponentUI::RenderAudioSource(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
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
void ComponentUI::RenderLayer2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    EditorUI::BeginPropertySection({ "Layer" });
    // Try to render as a dropdown of known layers from the active scene
    int currentId = static_cast<int>(data.value("Id", 0));
    Scenes::Scene* scene = Engine::CORE ? Engine::CORE->GetSceneManager().GetActive() : nullptr;
    if (scene) {
        auto& lm = scene->GetLayers();
        auto layers = lm.ListLayers();
        if (!layers.empty()) {
            // Build name list and id mapping
            std::vector<std::string> names;
            std::vector<uint16_t> ids;
            names.reserve(layers.size()); ids.reserve(layers.size());
            int selIndex = -1;
            for (size_t i = 0; i < layers.size(); ++i) {
                ids.push_back(layers[i].first);
                names.push_back(layers[i].second);
                if (static_cast<int>(layers[i].first) == currentId) selIndex = static_cast<int>(i);
            }

            std::vector<const char*> cstrs;
            cstrs.reserve(names.size());
            for (auto &s : names) cstrs.push_back(s.c_str());

            int displayIndex = selIndex >= 0 ? selIndex : 0;
            ImGui::Text("Id");
            ImGui::SameLine();
            ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::Combo("##LayerId", &displayIndex, cstrs.data(), static_cast<int>(cstrs.size()))) {
                data["Id"] = ids[displayIndex];
            }

            EditorUI::EndPropertySection();
            return;
        }
    }

    // Fallback: render raw integer if no scene or layers available
    EditorUI::RenderIntProperty("Id", data, "Id");
    EditorUI::EndPropertySection();
}

// Renders Material2D component UI for assigning textures and tweaking material properties
void ComponentUI::RenderMaterial2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;

    EditorUI::BeginPropertySection({ "Normal Map", "MRA Map", "Metallic", "Smoothness", "AO Strength", "Normal Strength", "Alpha Cutoff", "Flags" });

    // Helper lambda to render a texture slot with drag-and-drop
    auto RenderTextureSlot = [&](const char* label, const char* pathKey, const char* idKey) {
        // Fetch texture path from serialized data (empty if none)
        std::string texPath = data.value(pathKey, "");
        std::string valueText;

        if (!texPath.empty()) {
            // Display only the filename, not the full path
            valueText = std::filesystem::path(texPath).filename().string();
            // Handle texture reloading if ID is missing (e.g. after scene reload)
            uint32_t currentId = data.value(idKey, 0u);
            if (currentId == 0) {
                // Prevent repeated reload attempts every frame
                std::string attemptKey = std::string("_LoadAttempt_") + pathKey;
                if (!data.contains(attemptKey)) {
                    // Attempt to fetch texture from resource manager
                    auto tex = RM.Get<Texture>(texPath);
                    if (tex) {
                        data[idKey] = static_cast<uint32_t>(tex->ID());
                        LOG_DEBUG("Reloaded material texture: " << texPath);
                    }
                    // Mark reload attempt so we don't spam reloads
                    data[attemptKey] = true;
                }
            }
        }
        else {
            // Default placeholder text when no texture is assigned
            valueText = "None (drag texture here)";
        }

        // Render a read-only row displaying the current texture
        EditorUI::RenderStaticValueRow(label, valueText, texPath.empty());

        // Drag-and-drop target for texture assignment
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                // Extract dropped file path
                std::string droppedPath = static_cast<const char*>(payLoad->Data);

                // Validate file extension
                auto ext = std::filesystem::path(droppedPath).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                // Accept only image formats
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                    auto tex = RM.Get<Texture>(droppedPath);
                    if (tex) {
                        // Store texture ID and path in serialized material data
                        data[idKey] = static_cast<uint32_t>(tex->ID());
                        data[pathKey] = droppedPath;
                    }
                }
            }
            // Lowkey copied from audio
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
                        data[idKey] = static_cast<uint32_t>(tex->ID());
                        data[pathKey] = path;
                    }
                    break; // Only accept the first valid texture
                }
            }
            ImGui::EndDragDropTarget();
        }
    };

    // Texture slots
    RenderTextureSlot("Normal Map", "NormalTexturePath", "NormalTextureId");
    RenderTextureSlot("MRA Map", "MRA_TexturePath", "MRA_TextureId");

    // Scalar material properties
    EditorUI::RenderFloatRow("Metallic", "", data, "Metallic", 0.01f, 0.0f, 1.0f);
    EditorUI::RenderFloatRow("Smoothness", "", data, "Smoothness", 0.01f, 0.0f, 1.0f);
    EditorUI::RenderFloatRow("AO Strength", "", data, "AOStrength", 0.01f, 0.0f, 5.0f);
    EditorUI::RenderFloatRow("Normal Strength", "", data, "NormalStrength", 0.01f, 0.0f, 5.0f);
    EditorUI::RenderFloatRow("Alpha Cutoff", "", data, "AlphaCutoff", 0.01f, 0.0f, 1.0f);

    // Bitmask/flag-based material options
    EditorUI::RenderIntProperty("Flags", data, "Flags");

    EditorUI::EndPropertySection();
}
