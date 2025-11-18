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

#include "../editor/ComponentPropertyEditor.h"
#include "../editor/ComponentWidgets.h"
#include "core/Logger.h"
#include <imgui.h>
#include <filesystem>
#include "ecs/Components.h"
#include "serialization/EntitySerializer.h"
#include "services/ResourceManager.h"
#include <algorithm>
#include "../editor/AudioAssetLibrary.h"

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

// Renders the LocalTransform component properties
void ComponentUI::RenderLocalTransform(nlohmann::json& data) {
    // Start a grouped section so all three rows share aligned labels
    EditorUI::BeginPropertySection({ "Local Rotation", "Local Position", "Local Scale" });

    // Draw rotation as a quaternion with X Y Z W components
    // data["Rotation"] is expected to be a JSON object containing these keys
    EditorUI::RenderQuaternionRow("Local Rotation", data["Rotation"], "X", "Y", "Z", "W", 0.1f);

    // Draw position as a 3D vector with X Y Z fields
    EditorUI::RenderVector3DRow("Local Position", data["Position"], "X", "Y", "Z", 1.0f);

    // Draw scale as a 3D vector with X Y Z fields
    // Smaller dragSpeed so scaling changes are more precise
    EditorUI::RenderVector3DRow("Local Scale", data["Scale"], "X", "Y", "Z", 0.01f);

    // Close the grouped section and restore layout state
    EditorUI::EndPropertySection();
}

// Renders the SpriteRenderer2D component properties
void ComponentUI::RenderSpriteRenderer2D(nlohmann::json& data) {
    // Group all sprite related rows under one aligned section
    EditorUI::BeginPropertySection({ "Sprite", "Color", "Tiling", "Offset" });

    // Build a human readable summary of the current texture
    // TextureId is numeric and TexturePath is the file path string
    std::string valueText = "TextureId: " + std::to_string(data.value("TextureId", 0u));
    std::string texPath = data.value("TexturePath", "");
    if (!texPath.empty()) {
        // Show only the file name instead of the full path for readability
        valueText += " (" + std::filesystem::path(texPath).filename().string() + ")";
    }

    // Show the sprite information in a read only row
    EditorUI::RenderStaticValueRow("Sprite", valueText);

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
        ImGui::EndDragDropTarget();
    }

    // If a valid texture was dropped, show a small success message inline
    if (dropped) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Texture updated");
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
    EditorUI::BeginPropertySection({ "Sprite Sheet", "Frame Size", "Sheet Size", "Animation", "Playback" });

    // Build a human readable summary of the current texture (same as SpriteRenderer2D)
    std::string valueText = "TextureId: " + std::to_string(data.value("TextureId", 0u));
    std::string texPath = data.value("TexturePath", "");
    if (!texPath.empty()) {
        // Show only the file name instead of the full path for readability
        valueText += " (" + std::filesystem::path(texPath).filename().string() + ")";
    }

    // Show the sprite sheet information in a read only row
    EditorUI::RenderStaticValueRow("Sprite Sheet", valueText);

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

// Renders the SpriteFlip2D component properties
// Allows mirroring sprites horizontally and vertically
void ComponentUI::RenderSpriteFlip2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Flip" });

    // Two flip toggles on one row for compact display
    EditorUI::RenderCheckboxRow("Flip", data, "FlipX", "Horiz.", "FlipY", "Vert.");
    EditorUI::EndPropertySection();
}

// Renders the ZIndex2D component properties
// Controls the rendering order in 2D (higher values render on top)
void ComponentUI::RenderZIndex2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Z-Order" });

    // Integer Z-order value (can be negative)
    EditorUI::RenderIntProperty("Z-Order", data, "ZOrder");
    EditorUI::EndPropertySection();
}

// Static variab/flags
static bool s_showUnsupportedPopup = false;
static std::string s_unsupportedPath;


//Renders the AudioSource component Properties
void ComponentUI::RenderAudioSource(nlohmann::json& data)
{
    // Ensure keys exist with defaults
    if (!data.contains("CueId"))   data["CueId"] = 0;
    if (!data.contains("Volume"))  data["Volume"] = 1.0f;
    if (!data.contains("Pitch"))   data["Pitch"] = 1.0f;
    if (!data.contains("Loop"))    data["Loop"] = false;
    if (!data.contains("PlayOnStart")) data["PlayOnStart"] = false;
    if (!data.contains("Spatial3D"))   data["Spatial3D"] = false;

    // Get current CueID and set it to clip.Id from audiolibrary
    uint32_t cueId = data.value("CueId", 0u);

    // Get static reference of AudioAssetlib via getter
    auto& lib = AudioAssetLibrary::Get();  

    // Set clips to get clips struct info from a storage vector via getallclips function
    const auto& clips = lib.GetAllClips(); 

    const AudioAssetLibrary::ClipInfo* selectedClip = nullptr;

    //Iterate through the Clips to access specific clip ID 
    for (const auto& clip : clips)
    {
        if (clip.id == cueId)
        {
            // selected Clip is set
            selectedClip = &clip;
            break;
        }
    }

    // Label to show in the combo box
    std::string currentLabel;
    if (!selectedClip)
        currentLabel = "None (Audio Clip)";
    else
        currentLabel = selectedClip->name;


    // Group the properties visually
    EditorUI::BeginPropertySection({ "Audio Clip", "Volume", "Pitch", "Loop" });

    ImGui::TextUnformatted("Audio Clip");
    ImGui::SameLine();

    if (ImGui::BeginCombo("##AudioClipCombo", currentLabel.c_str()))
    {
        // "None" option
        bool isNone = (cueId == 0);
        if (ImGui::Selectable("None (Audio Clip)", isNone))
        {
            data["CueId"] = 0;
            cueId = 0;
        }
        if (isNone)
            ImGui::SetItemDefaultFocus();

        // One option per known clip
        for (const auto& clip : clips)
        {
            bool isSelected = (clip.id == cueId);
            if (ImGui::Selectable(clip.name.c_str(), isSelected))
            {
                data["CueId"] = clip.id;
                cueId = clip.id;
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    // Assign cueID from Aud library
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
        {
            const char* droppedPath = static_cast<const char*>(payload->Data);
            std::filesystem::path path(droppedPath);

            // Simple extension check here (since the library itself only filters in Refresh)
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            bool supported =
                (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac");

            if (supported)
            {
                // This will either return existing ClipInfo or register a new one
                const auto& clipInfo = lib.Register(path.string());
                data["CueId"] = clipInfo.id;
            }
            else
            {
                s_showUnsupportedPopup = true;
                s_unsupportedPath = path.string();
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Volume + Pitch sliders
    EditorUI::RenderFloatRow("Volume", "", data, "Volume", 0.05f);
    EditorUI::RenderFloatRow("Pitch", "", data, "Pitch", 0.05f);

    // Loop checkbox
    EditorUI::RenderCheckboxProperty("Loop", data, "Loop");

    // to be done edits.. missing stuff here
    EditorUI::RenderCheckboxProperty("Play On Start", data, "PlayOnStart");
    EditorUI::RenderCheckboxProperty("Spatial 3D", data, "Spatial3D");

    EditorUI::EndPropertySection();

    if (s_showUnsupportedPopup)
    {
        ImGui::OpenPopup("Unsupported Audio Format");
        ImGui::SetNextWindowSize(ImVec2(450, 250), ImGuiCond_Appearing);
    }

    if (ImGui::BeginPopupModal("Unsupported Audio Format",
        nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "The file you tried to assign is not a supported audio format.\n\n"
            "Only .wav, .ogg, .mp3, and .flac files are allowed.\n\n"
        );

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            s_showUnsupportedPopup = false;
            s_unsupportedPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

// Renders the Layer component properties
void ComponentUI::RenderLayer2D(nlohmann::json& data) {
    EditorUI::BeginPropertySection({ "Layer" });
    EditorUI::RenderIntProperty("Id", data, "Id");
    EditorUI::EndPropertySection();
}
