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
#include <functional>
#include <unordered_set>
#include <cstdio>
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

    const std::unordered_set<std::string> kImageExtensions = { ".png", ".jpg", ".jpeg" };
    const std::unordered_set<std::string> kFontExtensions = { ".ttf", ".otf", ".ttc" };
    const std::unordered_set<std::string> kAudioExtensions = { ".wav", ".ogg", ".mp3", ".flac" };
    static bool s_showAssetDropError = false;
    static std::string s_assetDropErrorMessage;

    std::string GetLowercaseExtension(const std::string& path) {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }

    // Builds a comma-separated list of extensions for validation messages
    std::string FormatExtensionList(const std::unordered_set<std::string>& extensions) {
        std::string out;
        for (const auto& ext : extensions) {
            if (!out.empty()) {
                out += ", ";
            }
            out += ext;
        }
        return out;
    }

    // Queue a modal popup that explains why a dropped asset was rejected
    void QueueAssetDropError(const std::string& path, const std::unordered_set<std::string>& allowedExtensions) {
        const std::string ext = GetLowercaseExtension(path);
        s_assetDropErrorMessage = "Unsupported format: " + (ext.empty() ? std::string("<unknown>") : ext) +
            ". Supported: " + FormatExtensionList(allowedExtensions);
        s_showAssetDropError = true;
        ImGui::OpenPopup("Unsupported Asset Format");
    }

    // Draws the modal popup if a drag/drop rejection was queued
    void RenderAssetDropErrorPopup() {
        if (!s_showAssetDropError) {
            return;
        }
        if (ImGui::BeginPopupModal("Unsupported Asset Format", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", s_assetDropErrorMessage.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Close")) {
                s_showAssetDropError = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Renders a small inline preview thumbnail for a texture ID
    void RenderInlineTexturePreview(uint32_t textureId, const char* tooltip) {
        if (textureId == 0) {
            return;
        }
        const float previewSize = 36.0f;
        const float rightEdge = ImGui::GetWindowContentRegionMax().x;
        const float previewX = rightEdge - previewSize - ImGui::GetStyle().FramePadding.x;

        ImGui::SameLine();
        ImGui::SetCursorPosX(previewX);
        ImGui::Image((ImTextureID)(uintptr_t)textureId,
            ImVec2(previewSize, previewSize), ImVec2(0, 1), ImVec2(1, 0));
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
    }

    bool HandleAssetDragDropTarget(const std::unordered_set<std::string>& allowedExtensions,
        const std::function<bool(const std::string&)>& onAccepted,
        const std::function<void(const std::string&)>& onRejected = nullptr) {
        if (!ImGui::BeginDragDropTarget()) {
            return false;
        }

        bool accepted = false;
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* droppedPath = static_cast<const char*>(payload->Data);
            if (droppedPath && *droppedPath) {
                std::string path(droppedPath);
                std::string ext = GetLowercaseExtension(path);
                if (!allowedExtensions.empty() && allowedExtensions.find(ext) == allowedExtensions.end()) {
                    if (onRejected) {
                        onRejected(path);
                    }
                } else {
                    accepted = onAccepted(path);
                }
            }
        }

        if (!accepted) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
                const char* dataBuf = static_cast<const char*>(payload->Data);
                const char* end = dataBuf + payload->DataSize;
                std::string firstPath;
                while (dataBuf < end) {
                    std::string path(dataBuf);
                    dataBuf += path.size() + 1;
                    if (path.empty()) {
                        continue;
                    }
                    if (firstPath.empty()) {
                        firstPath = path;
                    }
                    std::string ext = GetLowercaseExtension(path);
                    if (!allowedExtensions.empty() && allowedExtensions.find(ext) == allowedExtensions.end()) {
                        continue;
                    }
                    accepted = onAccepted(path);
                    break;
                }
                if (!accepted && !firstPath.empty() && onRejected) {
                    onRejected(firstPath);
                }
            }
        }

        ImGui::EndDragDropTarget();
        return accepted;
    }

    struct ImGuiIdScope {
        explicit ImGuiIdScope(const char* id) {
            ImGui::PushID(id);
        }
        ~ImGuiIdScope() {
            ImGui::PopID();
        }
    };

    bool RenderClearTrashButton(const char* id, const char* tooltip, ImFont* symbolsFont) {
        ImGui::SameLine();

        // Vertically center the button with the surrounding text
        const float lineHeight = ImGui::GetTextLineHeight();
        const float frameHeight = ImGui::GetFrameHeight();
        const float y = ImGui::GetCursorPosY();

        // Adjust cursor position to vertically center the button
        // Style adjustments for button appearance
        ImGui::SetCursorPosY(y - (frameHeight - lineHeight) * 0.5f);
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::DangerText);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

        // Render the clear trash button with optional symbols font
        if (symbolsFont) ImGui::PushFont(symbolsFont);
        const char* icon = symbolsFont ? "\xEE\xA1\xB2" : "X";
        const bool clicked = ImGui::SmallButton(icon);
        if (symbolsFont) ImGui::PopFont();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        ImGui::PopID();
        return clicked;
    }

    void UpdateSpriteAnimationPreview(nlohmann::json& animData, ECS::Entity entity, ECS::World* world) {
        if (!world || entity.IsNull() || !world->IsAlive(entity))
            return;
        if (!Editor::ECSUtils::HasComponent(world, entity, "SpriteRenderer2D"))
            return;

        const int frameWidth = animData.value("FrameWidth", 0);
        const int frameHeight = animData.value("FrameHeight", 0);
        const int sheetWidth = animData.value("SheetWidth", 0);
        const int sheetHeight = animData.value("SheetHeight", 0);
        if (frameWidth <= 0 || frameHeight <= 0 || sheetWidth <= 0 || sheetHeight <= 0)
            return;

        const int totalCols = sheetWidth / frameWidth;
        const int totalRows = sheetHeight / frameHeight;
        if (totalCols <= 0 || totalRows <= 0)
            return;

        const bool useRow = animData.value("UseRow", false);
        int windowStart = 0;
        int windowCount = 0;

        if (useRow) {
            const int rowIndex = std::clamp(animData.value("Row", 0), 0, totalRows - 1);
            const int startCol = std::clamp(animData.value("FrameOffset", 0), 0, totalCols - 1);
            const int available = totalCols - startCol;
            int rowCount = animData.value("FrameLength", 0);
            if (rowCount <= 0 || rowCount > available)
                rowCount = available;
            windowStart = rowIndex * totalCols + startCol;
            windowCount = rowCount;
        } else {
            windowStart = std::max(0, animData.value("StartFrame", 0));
            windowCount = animData.value("FrameCount", 0);
            if (windowCount <= 0) {
                const int totalFrames = totalCols * totalRows;
                windowCount = std::max(1, totalFrames - windowStart);
            }
        }

        if (windowCount <= 0)
            return;

        int localFrame = 0;
        if (Editor::ECSUtils::HasComponent(world, entity, "AnimationState2D")) {
            const auto* animState = Editor::ECSUtils::GetComponentPtr<ECS::Components::AnimationState2D>(world, entity, "AnimationState2D");
            if (animState) {
                localFrame = animState->CurrentFrame;
            }
        }
        localFrame = std::clamp(localFrame, 0, windowCount - 1);
        const int absoluteFrame = windowStart + localFrame;
        const int col = absoluteFrame % totalCols;
        const int row = absoluteFrame / totalCols;

        const float u0 = (col * frameWidth) / static_cast<float>(sheetWidth);
        const float v0 = (row * frameHeight) / static_cast<float>(sheetHeight);
        const float u1 = ((col + 1) * frameWidth) / static_cast<float>(sheetWidth);
        const float v1 = ((row + 1) * frameHeight) / static_cast<float>(sheetHeight);

        auto* sprite = Editor::ECSUtils::GetComponentPtr<ECS::Components::SpriteRenderer2D>(world, entity, "SpriteRenderer2D");
        if (!sprite) {
            return;
        }
        const uint32_t textureId = animData.value("TextureId", 0u);
        if (textureId != 0)
            sprite->TextureId = textureId;
        const uint32_t normalId = animData.value("NormalTextureId", 0u);
        if (normalId != 0)
            sprite->NormalTextureId = normalId;
        sprite->TextureFilter = static_cast<Graphics::TextureFilter>(
            animData.value("TextureFilter", 0));
        sprite->Width = frameWidth;
        sprite->Height = frameHeight;
        sprite->Tiling = Vector2D{ u1 - u0, v1 - v0 };
        sprite->Offset = Vector2D{ u0, v0 };
    }

    std::unordered_set<uint32_t> s_animPreviewedEntities;

    /**
     * @brief Builds default tag mask names ("Tag 0", "Tag 1", ..., "Tag 31").
     * @return Vector of tag mask names.
     */
    std::vector<std::string> BuildTagMaskNames() {
        std::vector<std::string> names;
        names.reserve(32);
        for (int i = 0; i < 32; ++i) {
            names.emplace_back("Tag " + std::to_string(i)); // Default name
        }
        return names;
    }

    /**
     * @brief Builds layer mask names based on the current scene's layers.
     * Defaults to "Layer 0", "Layer 1", ..., "Layer 31" if no custom names exist.
     * @return Vector of layer mask names.
     */
    std::vector<std::string> BuildLayerMaskNames() {
        std::vector<std::string> names(32);
        for (int i = 0; i < 32; ++i) {
            names[i] = "Layer " + std::to_string(i); // Default name
        }

        // Get active scene
        Scenes::Scene* scene = Engine::CORE ? Engine::CORE->GetSceneManager().GetActive() : nullptr;
        if (!scene) {
            return names; // No active scene, return defaults
        }

        // Override with actual layer names from the scene
        auto layers = scene->GetLayers().ListLayers();
        for (const auto& entry : layers) {
            if (entry.first < 32 && !entry.second.empty()) {
                names[entry.first] = entry.second; // Use custom layer name
            }
        }
        return names;
    }

    /**
     * @brief Builds generic flag names with a given prefix ("<prefix> 0", "<prefix> 1", ..., "<prefix> 31").
     * @param prefix The prefix for each flag name.
     * @return Vector of generic flag names.
     */
    std::vector<std::string> BuildGenericFlagNames(const char* prefix) {
        std::vector<std::string> names;
        names.reserve(32);
        for (int i = 0; i < 32; ++i) {
            names.emplace_back(std::string(prefix) + " " + std::to_string(i));
        }
        return names;
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

    // Share the symbols font with EditorUI helpers for icon-only reset buttons
    EditorUI::SetSymbolsFont(symbolsFont);
}

// Render any queued drag/drop validation popups
void ComponentUI::RenderAssetDropFeedbackPopup() {
    RenderAssetDropErrorPopup();
}

// -----------------------------------------------------------------------------
// Component Rendering
// -----------------------------------------------------------------------------

// Renders the Name component properties
void ComponentUI::RenderName(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("Name");
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
    ImGuiIdScope id("Active");
    EditorUI::BeginPropertySection({ "Active" });
    EditorUI::RenderCheckboxProperty("Enabled", data, "Enabled");
    EditorUI::EndPropertySection();
}

// Renders the TagMask component properties
void ComponentUI::RenderTagMask(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("TagMask");
    EditorUI::BeginPropertySection({ "Tag Mask" });

    static const std::vector<std::string> kTagNames = BuildTagMaskNames();
    EditorUI::RenderBitmaskDropdown("Mask", data, "Mask", kTagNames, 0u);

    EditorUI::EndPropertySection();
}

// Renders the LocalTransform component properties
void ComponentUI::RenderLocalTransform(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("LocalTransform");
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
    ImGuiIdScope id("SpriteRenderer2D");
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

    std::string normalPath = data.value("NormalTexturePath", "");
    std::string normalValueText;
    if (!normalPath.empty()) {
        normalValueText = std::filesystem::path(normalPath).filename().string();
        uint32_t currentNormalId = data.value("NormalTextureId", 0u);
        if (currentNormalId == 0) {
            if (!data.contains("_NormalTextureLoadAttempted")) {
                auto normalTex = RM.Get<Texture>(normalPath);
                if (normalTex) {
                    data["NormalTextureId"] = static_cast<uint32_t>(normalTex->ID());
                    LOG_DEBUG("Reloaded normal map from path: " << normalPath << ", id=" << normalTex->ID());
                }
                else {
                    LOG_WARNING("Failed to reload normal map from path: " << normalPath);
                }
                data["_NormalTextureLoadAttempted"] = true;
            }
        }
    }
    else {
        normalValueText = "None (drag normal map here)";
    }

    // Emissive map
    std::string emissivePath = data.value("EmissiveTexturePath", "");
    std::string emissiveValueText;

    // Try to reload emissive texture if path is set but ID is 0
    if (!emissivePath.empty()) {
        // Show only the file name
        emissiveValueText = std::filesystem::path(emissivePath).filename().string();
        uint32_t currentEmissiveId = data.value("EmissiveTextureId", 0u); // Get current ID

        // Only try to reload if ID is 0
        if (currentEmissiveId == 0) {
            if (!data.contains("_EmissiveTextureLoadAttempted")) {
                auto emissiveTex = RM.Get<Texture>(emissivePath); // Try to load texture

                // If successful, store ID in JSON
                if (emissiveTex) {
                    data["EmissiveTextureId"] = static_cast<uint32_t>(emissiveTex->ID());
                    LOG_DEBUG("Reloaded emissive map from path: " << emissivePath << ", id=" << emissiveTex->ID());
                }
                else {
                    LOG_WARNING("Failed to reload emissive map from path: " << emissivePath);
                }
                data["_EmissiveTextureLoadAttempted"] = true;
            }
        }
    }
    else {
        emissiveValueText = "None (drag emissive map here)";
    }

    // Group all sprite related rows under one aligned section
    EditorUI::BeginPropertySection({ "Sprite", "Texture Filter", "Normal Map", "Emissive Map", "Emissive Strength",
        "Color", "Tiling", "Offset" });

    // Show the sprite information in a read only row
    EditorUI::RenderStaticValueRow("Sprite", valueText, texPath.empty());
    if (!texPath.empty() && RenderClearTrashButton("SpriteClear", "Clear sprite", m_symbolsFont)) {
        data["TextureId"] = 0;
        data["TexturePath"] = "";
        data["Width"] = 0;
        data["Height"] = 0;
    }
    // Inline thumbnail preview to confirm the assigned sprite quickly
    if (EditorUI::PropertyFilterAllows("Sprite")) {
        RenderInlineTexturePreview(data.value("TextureId", 0u), "Sprite preview");
    }

    const bool dropped = HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
        auto tex = RM.Get<Texture>(droppedPath);
        if (tex) {
            data["TextureId"] = static_cast<uint32_t>(tex->ID());
            data["TexturePath"] = droppedPath;
            data["Width"] = tex->Width();
            data["Height"] = tex->Height();
            LOG_INFO("Dropped texture: " << droppedPath << ", id=" << tex->ID());
            return true;
        }
        LOG_ERROR("Failed to load dropped texture: " << droppedPath);
        return false;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kImageExtensions);
    });

    // If a valid texture was dropped, show a small success message inline
    if (dropped) {
        ImGui::SameLine();
        ImGui::TextColored(EditorStyle::SuccessText, "Texture updated");
    }

    const char* filterLabels[] = { "Nearest", "Linear" };
    int filter = data.value("TextureFilter", 0);
    filter = std::clamp(filter, 0, 1);
    ImGui::Text("Texture Filter");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##SpriteTextureFilter", filterLabels[filter])) {
        for (int i = 0; i < 2; ++i) {
            bool selected = (filter == i);
            if (ImGui::Selectable(filterLabels[i], selected)) {
                filter = i;
                data["TextureFilter"] = filter;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Normal map row
    EditorUI::RenderStaticValueRow("Normal Map", normalValueText, normalPath.empty());
    if (!normalPath.empty() && RenderClearTrashButton("NormalMapClear", "Clear normal map", m_symbolsFont)) {
        data["NormalTextureId"] = 0;
        data["NormalTexturePath"] = "";
    }
    // Inline thumbnail preview for the normal map
    if (EditorUI::PropertyFilterAllows("Normal Map")) {
        RenderInlineTexturePreview(data.value("NormalTextureId", 0u), "Normal map preview");
    }
    const bool droppedNormal = HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
        auto tex = RM.Get<Texture>(droppedPath);
        if (tex) {
            data["NormalTextureId"] = static_cast<uint32_t>(tex->ID());
            data["NormalTexturePath"] = droppedPath;
            LOG_INFO("Dropped normal map: " << droppedPath << ", id=" << tex->ID());
            return true;
        }
        LOG_ERROR("Failed to load dropped normal map: " << droppedPath);
        return false;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kImageExtensions);
    });

    if (droppedNormal) {
        ImGui::SameLine();
        ImGui::TextColored(EditorStyle::SuccessText, "Normal map updated");
    }

    // Emissive map row
    EditorUI::RenderStaticValueRow("Emissive Map", emissiveValueText, emissivePath.empty());
    // Inline thumbnail preview for the emissive map
    if (EditorUI::PropertyFilterAllows("Emissive Map")) {
        RenderInlineTexturePreview(data.value("EmissiveTextureId", 0u), "Emissive map preview");
    }
    const bool droppedEmissive = HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
        auto tex = RM.Get<Texture>(droppedPath);
        if (tex) {
            data["EmissiveTextureId"] = static_cast<uint32_t>(tex->ID());
            data["EmissiveTexturePath"] = droppedPath;
            LOG_INFO("Dropped emissive map: " << droppedPath << ", id=" << tex->ID());
            return true;
        }
        LOG_ERROR("Failed to load dropped emissive map: " << droppedPath);
        return false;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kImageExtensions);
    });

    if (droppedEmissive) {
        ImGui::SameLine();
        ImGui::TextColored(EditorStyle::SuccessText, "Emissive map updated");
    }

    // Emissive strength multiplier
    EditorUI::RenderFloatRow("Emissive Strength", "", data, "EmissiveStrength", 0.1f, 0.0f, 100.0f);

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
    ImGuiIdScope id("Rigidbody2D");
    // Group all rigidbody fields so labels line up and scrolling feels consistent
    EditorUI::BeginPropertySection({ "Mass", "Inverse Mass", "Linear Damping", "Angular Damping",
        "Gravity Scale", "Flags" });

    // Mass in kilograms
    EditorUI::RenderFloatRow("Mass", "kg", data, "Mass", 0.1f);

    // Inverse mass is derived from mass; keep it in sync and render read-only
    const float mass = data.value("Mass", 1.0f);
    const float invMass = (mass <= 0.0f) ? 0.0f : (1.0f / mass);
    data["InverseMass"] = invMass;
    char invBuf[64];
    std::snprintf(invBuf, sizeof(invBuf), "%.4f 1/kg", invMass);
    EditorUI::RenderStaticValueRow("Inverse Mass", invBuf);

    // Linear damping slows translational motion over time
    EditorUI::RenderFloatRow("Linear Damping", "", data, "LinearDamping", 0.01f);

    // Angular damping slows rotational motion over time
    EditorUI::RenderFloatRow("Angular Damping", "", data, "AngularDamping", 0.01f);

    // Gravity scale lets this body feel heavier or lighter than global gravity
    EditorUI::RenderFloatRow("Gravity Scale", "", data, "GravityScale", 0.1f);

    static const std::vector<std::string> kRigidbodyFlagNames = {
        "Kinematic",
        "Use Gravity",
        "Fixed Rotation"
    };
    EditorUI::RenderBitmaskDropdown("Flags", data, "Flags", kRigidbodyFlagNames, 0u);
    EditorUI::EndPropertySection();
}

// Renders the LinearVelocity2D component
void ComponentUI::RenderLinearVelocity2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("LinearVelocity2D");
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
    ImGuiIdScope id("AngularVelocity2D");
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
    ImGuiIdScope id("CircleCollider2D");
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
    const std::vector<std::string> layerNames = BuildLayerMaskNames();
    EditorUI::RenderBitmaskDropdown("Layer Mask##Circle", data, "LayerMask", layerNames, 0xFFFFFFFFu);
    EditorUI::EndPropertySection();
}

// Renders the BoxCollider2D component
void ComponentUI::RenderBoxCollider2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    ImGuiIdScope id("BoxCollider2D");
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
    const std::vector<std::string> layerNames = BuildLayerMaskNames();
    EditorUI::RenderBitmaskDropdown("Layer Mask##Box", data, "LayerMask", layerNames, 0xFFFFFFFFu);

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
    ImGuiIdScope id("ShapeCircle2D");
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
    ImGuiIdScope id("ShapeBox2D");
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
    ImGuiIdScope id("ShapeLine2D");
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
    ImGuiIdScope id("Camera3D");
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
    ImGuiIdScope id("Acceleration2D");
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
    ImGuiIdScope id("PhysicsMaterial2D");
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
    ImGuiIdScope id("SpriteSheetAnimation2D");
    const size_t hashBefore = std::hash<std::string>{}(data.dump());
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

    std::string normalPath = data.value("NormalTexturePath", "");
    std::string normalValueText;
    if (!normalPath.empty()) {
        normalValueText = std::filesystem::path(normalPath).filename().string();
        uint32_t currentNormalId = data.value("NormalTextureId", 0u);
        if (currentNormalId == 0) {
            if (!data.contains("_NormalTextureLoadAttempted")) {
                auto normalTex = RM.Get<Texture>(normalPath);
                if (normalTex) {
                    data["NormalTextureId"] = static_cast<uint32_t>(normalTex->ID());
                    LOG_DEBUG("Reloaded normal sheet from path: " << normalPath << ", id=" << normalTex->ID());
                }
                else {
                    LOG_WARNING("Failed to reload normal sheet from path: " << normalPath);
                }
                data["_NormalTextureLoadAttempted"] = true;
            }
        }
    }
    else {
        normalValueText = "None (drag normal sheet here)";
    }


    // Group all sprite sheet related rows under one aligned section
    EditorUI::BeginPropertySection({ "Sprite Sheet", "Texture Filter", "Normal Map", "Frame Width", "Frame Height",
        "Sheet Width", "Sheet Height", "Mode", "Start Frame", "Frame Count", "Row", "Frame Offset", "Frame Length",
        "FPS", "Loop", "Playing" });

    // Show the sprite sheet information in a read only row
    EditorUI::RenderStaticValueRow("Sprite Sheet", valueText, texPath.empty());
    if (!texPath.empty() && RenderClearTrashButton("SpriteSheetClear", "Clear sprite sheet", m_symbolsFont)) {
        data["TextureId"] = 0;
        data["TexturePath"] = "";
        data["SheetWidth"] = 0;
        data["SheetHeight"] = 0;
    }
    // Inline thumbnail preview for the sprite sheet texture
    if (EditorUI::PropertyFilterAllows("Sprite Sheet")) {
        RenderInlineTexturePreview(data.value("TextureId", 0u), "Sprite sheet preview");
    }

    const bool dropped = HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
        auto tex = RM.Get<Texture>(droppedPath);
        if (tex) {
            data["TextureId"] = static_cast<uint32_t>(tex->ID());
            data["TexturePath"] = droppedPath;
            data["SheetWidth"] = tex->Width();
            data["SheetHeight"] = tex->Height();
            LOG_INFO("Dropped sprite sheet: " << droppedPath << ", id=" << tex->ID());
            return true;
        }
        LOG_ERROR("Failed to load dropped sprite sheet: " << droppedPath);
        return false;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kImageExtensions);
    });
    const char* filterLabels[] = { "Nearest", "Linear" };
    int filter = data.value("TextureFilter", 0);
    filter = std::clamp(filter, 0, 1);
    ImGui::Text("Texture Filter");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##SpriteSheetTextureFilter", filterLabels[filter])) {
        for (int i = 0; i < 2; ++i) {
            bool selected = (filter == i);
            if (ImGui::Selectable(filterLabels[i], selected)) {
                filter = i;
                data["TextureFilter"] = filter;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    (void)dropped; // suppress for now, could be used for feedback

    // Normal map row
    EditorUI::RenderStaticValueRow("Normal Map", normalValueText, normalPath.empty());
    if (!normalPath.empty() && RenderClearTrashButton("SpriteSheetNormalClear", "Clear normal map", m_symbolsFont)) {
        data["NormalTextureId"] = 0;
        data["NormalTexturePath"] = "";
    }
    // Inline thumbnail preview for the normal sheet
    if (EditorUI::PropertyFilterAllows("Normal Map")) {
        RenderInlineTexturePreview(data.value("NormalTextureId", 0u), "Normal sheet preview");
    }
    const bool droppedNormal = HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
        auto tex = RM.Get<Texture>(droppedPath);
        if (tex) {
            data["NormalTextureId"] = static_cast<uint32_t>(tex->ID());
            data["NormalTexturePath"] = droppedPath;
            LOG_INFO("Dropped normal sheet: " << droppedPath << ", id=" << tex->ID());
            return true;
        }
        LOG_ERROR("Failed to load dropped normal sheet: " << droppedPath);
        return false;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kImageExtensions);
    });

    if (droppedNormal) {
        ImGui::SameLine();
        ImGui::TextColored(EditorStyle::SuccessText, "Normal map updated");
    }

    // Individual frame dimensions
    EditorUI::RenderIntProperty("Frame Width", data, "FrameWidth");
    EditorUI::RenderIntProperty("Frame Height", data, "FrameHeight");

    // Total sprite sheet dimensions (auto-filled when texture is dropped)
    EditorUI::RenderIntProperty("Sheet Width", data, "SheetWidth");
    EditorUI::RenderIntProperty("Sheet Height", data, "SheetHeight");

    int mode = data.value("UseRow", false) ? 1 : 0;
    const char* modes[] = { "Frame Window", "Row" };
    ImGui::Text("Mode");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo("##AnimMode", &mode, modes, 2)) {
        data["UseRow"] = (mode == 1);
    }

    const bool useRow = data.value("UseRow", false);
    if (!useRow) {
        // Which frame to start the animation from
        EditorUI::RenderIntProperty("Start Frame", data, "StartFrame");

        // How many frames in the animation sequence
        EditorUI::RenderIntProperty("Frame Count", data, "FrameCount");
    } else {
        EditorUI::RenderIntProperty("Row", data, "Row");
        EditorUI::RenderIntProperty("Frame Offset", data, "FrameOffset");
        EditorUI::RenderIntProperty("Frame Length", data, "FrameLength");
    }

    // Animation speed in frames per second
    EditorUI::RenderFloatRow("FPS", "", data, "FramesPerSecond", 0.5f);

    // Playback controls
    EditorUI::RenderCheckboxProperty("Loop", data, "Loop");
    EditorUI::RenderCheckboxProperty("Playing", data, "Playing");

    EditorUI::EndPropertySection();

    const size_t hashAfter = std::hash<std::string>{}(data.dump());
    const bool needsInitialPreview = world && !entity.IsNull() &&
        s_animPreviewedEntities.find(entity.Index) == s_animPreviewedEntities.end();

    if (hashAfter != hashBefore || needsInitialPreview) {
        UpdateSpriteAnimationPreview(data, entity, world);
        if (world && !entity.IsNull() && world->IsAlive(entity)) {
            s_animPreviewedEntities.insert(entity.Index);
        }
    }
}

// Renders the ZIndex2D component properties
// Controls the rendering order in 2D (higher values render on top)
void ComponentUI::RenderZIndex2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("ZIndex2D");
    EditorUI::BeginPropertySection({ "Z-Order" });

    // Integer Z-order value (can be negative)
    EditorUI::RenderIntProperty("Z-Order", data, "ZOrder");
    EditorUI::EndPropertySection();
}

// Renders the Light2D component properties
void ComponentUI::RenderLight2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("Light2D");
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

    const bool isPointLight = (lightType == 1);
    if (isPointLight) {
        // Position and range are used for Point lights
        EditorUI::RenderVector3DRow("Position##Light2D", data["Position"], "X", "Y", "Z", 0.1f);
        EditorUI::RenderFloatRow("Range##Light2D", "units", data, "Range", 0.5f);
    }
    else {
        // Direction is used for Directional lights
        EditorUI::RenderVector3DRow("Direction##Light2D", data["Direction"], "X", "Y", "Z", 0.1f);
    }

    // Color
    if (!data.contains("Color")) {
        data["Color"] = nlohmann::json{ {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    }
    EditorUI::RenderColorProperty("Color##Light2D", data["Color"]);

    // Intensity
    EditorUI::RenderFloatRow("Intensity##Light2D", "", data, "Intensity", 0.1f);

    // Casts Shadows
    EditorUI::RenderCheckboxProperty("Casts Shadows##Light2D", data, "CastsShadows");

    EditorUI::EndPropertySection();
}

// Renders the Text component properties
void ComponentUI::RenderText(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("Text");
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
    ImGuiIdScope id("AnimationState2D");
    EditorUI::BeginPropertySection({ "Current Frame", "Time Accumulator", "Finished" });

    // Current frame (integer)
    EditorUI::RenderIntProperty("Current Frame##AnimState", data, "CurrentFrame");

    // Time accumulator (float)
    EditorUI::RenderFloatRow("Time Accumulator##AnimState", "s", data, "TimeAccumulator", 0.01f);

    // Finished (boolean)
    EditorUI::RenderCheckboxProperty("Finished##AnimState", data, "Finished");

    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUICanvas(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUICanvas");

    if (!data.contains("ReferenceSize")) data["ReferenceSize"] = { {"X", 1920.0f}, {"Y", 1080.0f} };
    if (!data.contains("Offset")) data["Offset"] = { {"X", 0.0f}, {"Y", 0.0f} };
    if (!data.contains("ScaleMode")) data["ScaleMode"] = 0;

    EditorUI::BeginPropertySection({ "Reference Size", "Offset", "Scale Mode" });
    EditorUI::RenderVector2DRow("Reference Size", data["ReferenceSize"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Offset", data["Offset"], "X", "Y", 1.0f);

    const char* scaleModes[] = { "Fit", "Fill", "Match Width", "Match Height" };
    int scaleMode = data.value("ScaleMode", 0);
    scaleMode = std::max(0, std::min(scaleMode, 3));
    ImGui::Text("Scale Mode");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    if (ImGui::BeginCombo("##GUIScaleMode", scaleModes[scaleMode])) {
        for (int i = 0; i < 4; ++i) {
            bool selected = (scaleMode == i);
            if (ImGui::Selectable(scaleModes[i], selected)) {
                scaleMode = i;
                data["ScaleMode"] = scaleMode;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUIElement(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUIElement");

    if (!data.contains("Position")) data["Position"] = { {"X", 0.0f}, {"Y", 0.0f} };
    if (!data.contains("Size")) data["Size"] = { {"X", 100.0f}, {"Y", 100.0f} };
    if (!data.contains("Visible")) data["Visible"] = true;
    if (!data.contains("Alignment")) data["Alignment"] = 0;
    if (!data.contains("ZOrder")) data["ZOrder"] = 0;
    if (!data.contains("Margin")) data["Margin"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f} };
    if (!data.contains("Padding")) data["Padding"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f} };

    EditorUI::BeginPropertySection({ "Position", "Size", "Visible", "Alignment", "Z Order", "Margin", "Padding" });
    EditorUI::RenderVector2DRow("Position", data["Position"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Size", data["Size"], "X", "Y", 1.0f);
    EditorUI::RenderCheckboxProperty("Visible", data, "Visible");
    const char* alignmentOptions[] = {
        "Top Left", "Top", "Top Right",
        "Left", "Center", "Right",
        "Bottom Left", "Bottom", "Bottom Right"
    };
    int alignment = data.value("Alignment", 0);
    alignment = std::max(0, std::min(alignment, 8));
    ImGui::Text("Alignment");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    if (ImGui::BeginCombo("##GUIAlignment", alignmentOptions[alignment])) {
        for (int i = 0; i < 9; ++i) {
            bool selected = (alignment == i);
            if (ImGui::Selectable(alignmentOptions[i], selected)) {
                alignment = i;
                data["Alignment"] = alignment;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    EditorUI::RenderIntProperty("Z Order", data, "ZOrder");
    EditorUI::RenderVector4DRow("Margin", data["Margin"], "X", "Y", "Z", "W", 1.0f);
    EditorUI::RenderVector4DRow("Padding", data["Padding"], "X", "Y", "Z", "W", 1.0f);
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUIPanel(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUIPanel");

    if (!data.contains("Color")) data["Color"] = { {"R", 0.2f}, {"G", 0.2f}, {"B", 0.2f}, {"A", 1.0f} };
    if (!data.contains("CornerRadius")) data["CornerRadius"] = 0.0f;

    EditorUI::BeginPropertySection({ "Color", "Corner Radius" });
    EditorUI::RenderColorRow("Color", data["Color"]);
    EditorUI::RenderFloatRow("Corner Radius", "px", data, "CornerRadius", 0.1f);
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUIText(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUIText");

    if (!data.contains("Text")) data["Text"] = "Text";
    if (!data.contains("FontPath")) data["FontPath"] = "";
    if (!data.contains("Color")) data["Color"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    if (!data.contains("FontSize")) {
        if (data.contains("PixelSize")) {
            data["FontSize"] = data["PixelSize"];
        } else {
            data["FontSize"] = 24.0f;
        }
    }
    if (!data.contains("Wrap")) data["Wrap"] = false;
    if (!data.contains("HAlign")) data["HAlign"] = 0;
    if (!data.contains("VAlign")) data["VAlign"] = 0;

    EditorUI::BeginPropertySection({ "Text", "Font", "Color", "Font Size", "Wrap", "H Align", "V Align" });
    EditorUI::RenderTextProperty("Text", data, "Text");

    std::string fontPath = data.value("FontPath", std::string());
    std::string fontValueText;
    if (!fontPath.empty()) {
        fontValueText = std::filesystem::path(fontPath).filename().string();
    }
    else {
        fontValueText = "None (drag font here)";
    }

    EditorUI::RenderStaticValueRow("Font", fontValueText, fontPath.empty());
    if (!fontPath.empty() && RenderClearTrashButton("GUITextFontClear", "Clear font", m_symbolsFont)) {
        data["FontPath"] = "";
    }

    HandleAssetDragDropTarget(kFontExtensions, [&](const std::string& droppedPath) {
        data["FontPath"] = droppedPath;
        return true;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kFontExtensions);
    });

    EditorUI::RenderColorRow("Color", data["Color"]);
    EditorUI::RenderFloatRow("Font Size", "px", data, "FontSize", 1.0f);
    EditorUI::RenderCheckboxProperty("Wrap", data, "Wrap");

    const char* hAlignOptions[] = { "Left", "Center", "Right" };
    int hAlign = data.value("HAlign", 0);
    hAlign = std::max(0, std::min(hAlign, 2));
    ImGui::Text("H Align");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    if (ImGui::BeginCombo("##GUITextHAlign", hAlignOptions[hAlign])) {
        for (int i = 0; i < 3; ++i) {
            bool selected = (hAlign == i);
            if (ImGui::Selectable(hAlignOptions[i], selected)) {
                hAlign = i;
                data["HAlign"] = hAlign;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const char* vAlignOptions[] = { "Top", "Middle", "Bottom" };
    int vAlign = data.value("VAlign", 0);
    vAlign = std::max(0, std::min(vAlign, 2));
    ImGui::Text("V Align");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    if (ImGui::BeginCombo("##GUITextVAlign", vAlignOptions[vAlign])) {
        for (int i = 0; i < 3; ++i) {
            bool selected = (vAlign == i);
            if (ImGui::Selectable(vAlignOptions[i], selected)) {
                vAlign = i;
                data["VAlign"] = vAlign;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUIImage(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUIImage");

    if (!data.contains("TexturePath")) data["TexturePath"] = "";
    if (!data.contains("Color")) data["Color"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    if (!data.contains("UVRect")) data["UVRect"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 1.0f}, {"W", 1.0f} };
    if (!data.contains("ScaleMode")) data["ScaleMode"] = 0;
    if (!data.contains("TextureFilter")) data["TextureFilter"] = 0;
    if (!data.contains("UseSlicing")) data["UseSlicing"] = false;
    if (!data.contains("SliceBorder")) data["SliceBorder"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f} };

    EditorUI::BeginPropertySection({ "Texture", "Texture Filter", "Color", "UV Rect", "Scale Mode", "Use Slicing",
        "Slice Border" });

    std::string texturePath = data.value("TexturePath", std::string());
    std::string textureValueText;
    if (!texturePath.empty()) {
        textureValueText = std::filesystem::path(texturePath).filename().string();
    } else {
        textureValueText = "None (drag texture here)";
    }
    EditorUI::RenderStaticValueRow("Texture", textureValueText, texturePath.empty());
    if (!texturePath.empty() && RenderClearTrashButton("GUIImageTextureClear", "Clear texture", m_symbolsFont)) {
        data["TexturePath"] = "";
    }
    HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
        data["TexturePath"] = droppedPath;
        return true;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kImageExtensions);
    });

    const char* filterLabels[] = { "Nearest", "Linear" };
    int filter = data.value("TextureFilter", 0);
    filter = std::clamp(filter, 0, 1);
    ImGui::Text("Texture Filter");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##GUIImageTextureFilter", filterLabels[filter])) {
        for (int i = 0; i < 2; ++i) {
            bool selected = (filter == i);
            if (ImGui::Selectable(filterLabels[i], selected)) {
                filter = i;
                data["TextureFilter"] = filter;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    EditorUI::RenderColorRow("Color", data["Color"]);
    EditorUI::RenderVector4DRow("UV Rect", data["UVRect"], "X", "Y", "Z", "W", 0.01f);

    const char* scaleModes[] = { "Stretch", "Fit", "Fill" };
    int scaleMode = data.value("ScaleMode", 0);
    scaleMode = std::max(0, std::min(scaleMode, 2));
    ImGui::Text("Scale Mode");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    if (ImGui::BeginCombo("##GUIImageScaleMode", scaleModes[scaleMode])) {
        for (int i = 0; i < 3; ++i) {
            bool selected = (scaleMode == i);
            if (ImGui::Selectable(scaleModes[i], selected)) {
                scaleMode = i;
                data["ScaleMode"] = scaleMode;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    EditorUI::RenderCheckboxProperty("Use Slicing", data, "UseSlicing");
    EditorUI::RenderVector4DRow("Slice Border", data["SliceBorder"], "X", "Y", "Z", "W", 0.1f);
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUIInput(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUIInput");

    if (!data.contains("Hovered")) data["Hovered"] = false;
    if (!data.contains("Pressed")) data["Pressed"] = false;
    if (!data.contains("Clicked")) data["Clicked"] = false;
    if (!data.contains("Released")) data["Released"] = false;
    if (!data.contains("Dragging")) data["Dragging"] = false;
    if (!data.contains("Entered")) data["Entered"] = false;
    if (!data.contains("Exited")) data["Exited"] = false;

    EditorUI::BeginPropertySection({ "Hovered", "Pressed", "Clicked", "Released", "Dragging", "Entered", "Exited" });
    ImGui::BeginDisabled();
    EditorUI::RenderCheckboxProperty("Hovered", data, "Hovered");
    EditorUI::RenderCheckboxProperty("Pressed", data, "Pressed");
    EditorUI::RenderCheckboxProperty("Clicked", data, "Clicked");
    EditorUI::RenderCheckboxProperty("Released", data, "Released");
    EditorUI::RenderCheckboxProperty("Dragging", data, "Dragging");
    EditorUI::RenderCheckboxProperty("Entered", data, "Entered");
    EditorUI::RenderCheckboxProperty("Exited", data, "Exited");
    ImGui::EndDisabled();
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUIStateStyle(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUIStateStyle");

    if (!data.contains("NormalColor")) data["NormalColor"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    if (!data.contains("HoverColor")) data["HoverColor"] = { {"R", 0.9f}, {"G", 0.9f}, {"B", 0.9f}, {"A", 1.0f} };
    if (!data.contains("PressedColor")) data["PressedColor"] = { {"R", 0.8f}, {"G", 0.8f}, {"B", 0.8f}, {"A", 1.0f} };
    if (!data.contains("DisabledColor")) data["DisabledColor"] = { {"R", 0.6f}, {"G", 0.6f}, {"B", 0.6f}, {"A", 0.6f} };

    EditorUI::BeginPropertySection({ "Normal Color", "Hover Color", "Pressed Color", "Disabled Color" });
    EditorUI::RenderColorRow("Normal Color", data["NormalColor"]);
    EditorUI::RenderColorRow("Hover Color", data["HoverColor"]);
    EditorUI::RenderColorRow("Pressed Color", data["PressedColor"]);
    EditorUI::RenderColorRow("Disabled Color", data["DisabledColor"]);
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUIButton(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUIButton");

    if (!data.contains("Text")) data["Text"] = "Button";
    if (!data.contains("FontPath")) data["FontPath"] = "";
    if (!data.contains("IconPath")) data["IconPath"] = "";
    if (!data.contains("NormalColor")) data["NormalColor"] = { {"R", 0.25f}, {"G", 0.25f}, {"B", 0.25f}, {"A", 1.0f} };
    if (!data.contains("HoverColor")) data["HoverColor"] = { {"R", 0.35f}, {"G", 0.35f}, {"B", 0.35f}, {"A", 1.0f} };
    if (!data.contains("PressedColor")) data["PressedColor"] = { {"R", 0.15f}, {"G", 0.15f}, {"B", 0.15f}, {"A", 1.0f} };
    if (!data.contains("DisabledColor")) data["DisabledColor"] = { {"R", 0.2f}, {"G", 0.2f}, {"B", 0.2f}, {"A", 0.6f} };
    if (!data.contains("TextColor")) data["TextColor"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    if (!data.contains("IconColor")) data["IconColor"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    if (!data.contains("FontSize")) data["FontSize"] = 24.0f;
    if (!data.contains("CornerRadius")) data["CornerRadius"] = 0.0f;
    if (!data.contains("IconSize")) data["IconSize"] = { {"X", 24.0f}, {"Y", 24.0f} };
    if (!data.contains("IconOffset")) data["IconOffset"] = { {"X", 0.0f}, {"Y", 0.0f} };
    if (!data.contains("Padding")) data["Padding"] = { {"X", 8.0f}, {"Y", 6.0f}, {"Z", 8.0f}, {"W", 6.0f} };
    if (!data.contains("Disabled")) data["Disabled"] = false;
    if (!data.contains("Toggle")) data["Toggle"] = false;
    if (!data.contains("Toggled")) data["Toggled"] = false;

    EditorUI::BeginPropertySection({
        "Text", "Font", "Icon", "Normal Color", "Hover Color", "Pressed Color",
        "Disabled Color", "Text Color", "Icon Color", "Font Size", "Corner Radius",
        "Icon Size", "Icon Offset", "Padding", "Disabled", "Toggle", "Toggled"
    });

    EditorUI::RenderTextProperty("Text", data, "Text");

    std::string fontPath = data.value("FontPath", std::string());
    std::string fontValueText = fontPath.empty()
        ? "None (drag font here)"
        : std::filesystem::path(fontPath).filename().string();
    EditorUI::RenderStaticValueRow("Font", fontValueText, fontPath.empty());
    if (!fontPath.empty() && RenderClearTrashButton("GUIButtonFontClear", "Clear font", m_symbolsFont)) {
        data["FontPath"] = "";
    }
    HandleAssetDragDropTarget(kFontExtensions, [&](const std::string& droppedPath) {
        data["FontPath"] = droppedPath;
        return true;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kFontExtensions);
    });

    std::string iconPath = data.value("IconPath", std::string());
    std::string iconValueText = iconPath.empty()
        ? "None (drag icon here)"
        : std::filesystem::path(iconPath).filename().string();
    EditorUI::RenderStaticValueRow("Icon", iconValueText, iconPath.empty());
    if (!iconPath.empty() && RenderClearTrashButton("GUIButtonIconClear", "Clear icon", m_symbolsFont)) {
        data["IconPath"] = "";
    }
    HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
        data["IconPath"] = droppedPath;
        return true;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kImageExtensions);
    });

    EditorUI::RenderColorRow("Normal Color", data["NormalColor"]);
    EditorUI::RenderColorRow("Hover Color", data["HoverColor"]);
    EditorUI::RenderColorRow("Pressed Color", data["PressedColor"]);
    EditorUI::RenderColorRow("Disabled Color", data["DisabledColor"]);
    EditorUI::RenderColorRow("Text Color", data["TextColor"]);
    EditorUI::RenderColorRow("Icon Color", data["IconColor"]);
    EditorUI::RenderFloatRow("Font Size", "px", data, "FontSize", 1.0f);
    EditorUI::RenderFloatRow("Corner Radius", "px", data, "CornerRadius", 0.1f);
    EditorUI::RenderVector2DRow("Icon Size", data["IconSize"], "X", "Y", 1.0f);
    EditorUI::RenderVector2DRow("Icon Offset", data["IconOffset"], "X", "Y", 1.0f);
    EditorUI::RenderVector4DRow("Padding", data["Padding"], "X", "Y", "Z", "W", 1.0f);
    EditorUI::RenderCheckboxProperty("Disabled", data, "Disabled");
    EditorUI::RenderCheckboxProperty("Toggle", data, "Toggle");
    EditorUI::RenderCheckboxProperty("Toggled", data, "Toggled");
    EditorUI::EndPropertySection();
}

void ComponentUI::RenderGUISlider(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("GUISlider");

    if (!data.contains("Value")) data["Value"] = 0.0f;
    if (!data.contains("Min")) data["Min"] = 0.0f;
    if (!data.contains("Max")) data["Max"] = 1.0f;
    if (!data.contains("Step")) data["Step"] = 0.0f;
    if (!data.contains("TrackColor")) data["TrackColor"] = { {"R", 0.2f}, {"G", 0.2f}, {"B", 0.2f}, {"A", 1.0f} };
    if (!data.contains("FillColor")) data["FillColor"] = { {"R", 0.4f}, {"G", 0.4f}, {"B", 0.4f}, {"A", 1.0f} };
    if (!data.contains("KnobColor")) data["KnobColor"] = { {"R", 0.9f}, {"G", 0.9f}, {"B", 0.9f}, {"A", 1.0f} };
    if (!data.contains("CornerRadius")) data["CornerRadius"] = 0.0f;
    if (!data.contains("KnobSize")) data["KnobSize"] = { {"X", 16.0f}, {"Y", 16.0f} };
    if (!data.contains("Padding")) data["Padding"] = { {"X", 6.0f}, {"Y", 6.0f}, {"Z", 6.0f}, {"W", 6.0f} };
    if (!data.contains("Horizontal")) data["Horizontal"] = true;
    if (!data.contains("Disabled")) data["Disabled"] = false;
    if (!data.contains("ValueChanged")) data["ValueChanged"] = false;

    EditorUI::BeginPropertySection({
        "Value", "Min", "Max", "Step", "Track Color", "Fill Color", "Knob Color",
        "Corner Radius", "Knob Size", "Padding", "Horizontal", "Disabled", "Value Changed"
    });

    EditorUI::RenderFloatRow("Value", "", data, "Value", 0.01f);
    EditorUI::RenderFloatRow("Min", "", data, "Min", 0.01f);
    EditorUI::RenderFloatRow("Max", "", data, "Max", 0.01f);
    EditorUI::RenderFloatRow("Step", "", data, "Step", 0.01f);
    EditorUI::RenderColorRow("Track Color", data["TrackColor"]);
    EditorUI::RenderColorRow("Fill Color", data["FillColor"]);
    EditorUI::RenderColorRow("Knob Color", data["KnobColor"]);
    EditorUI::RenderFloatRow("Corner Radius", "px", data, "CornerRadius", 0.1f);
    EditorUI::RenderVector2DRow("Knob Size", data["KnobSize"], "X", "Y", 1.0f);
    EditorUI::RenderVector4DRow("Padding", data["Padding"], "X", "Y", "Z", "W", 1.0f);
    EditorUI::RenderCheckboxProperty("Horizontal", data, "Horizontal");
    EditorUI::RenderCheckboxProperty("Disabled", data, "Disabled");
    ImGui::BeginDisabled();
    EditorUI::RenderCheckboxProperty("Value Changed", data, "ValueChanged");
    ImGui::EndDisabled();
    EditorUI::EndPropertySection();
}

// Generic renderer for C# / unknown components. Uses EditorUI helpers where possible
// so the look & feel matches other component UIs.
void ComponentUI::RenderGenericComponent(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity; (void)world;
    ImGuiIdScope id("GenericComponent");

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

// Renders the AudioSource component Properties
void ComponentUI::RenderAudioSource(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    // Note: Defaults are now handled by EditorComponentRegistry, not here
    // This prevents JSON modification every frame which would mark component as dirty
    ImGuiIdScope id("AudioSource");
    // Ensure keys exist with defaults
    if (!data.contains("CueId"))       data["CueId"] = 0;
    if (!data.contains("CuePath"))     data["CuePath"] = "";
    if (!data.contains("Volume"))      data["Volume"] = 1.0f;
    if (!data.contains("Pitch"))       data["Pitch"] = 1.0f;
    if (!data.contains("Loop"))        data["Loop"] = false;
    if (!data.contains("PlayOnStart")) data["PlayOnStart"] = false;
    if (!data.contains("Spatial3D"))   data["Spatial3D"] = false;

    // SINGLE BeginPropertySection call with ALL field names for proper alignment
    // Note: Fade duration fields are conditionally shown, so we include all possible fields
    EditorUI::BeginPropertySection({ "Audio Clip", "Volume", "Pitch", "Loop", "Play On Start", "Spatial 3D", "Bus", "Pan",
                                     "Enable Fade In", "Fade In Duration", "Enable Fade Out", "Fade Out Duration" });

    uint32_t cueId = data.value("CueId", 0u);
    std::string cuePath = data.value("CuePath", std::string());
    auto& lib = AudioAssetLibrary::Get();

    const AudioAssetLibrary::ClipInfo* selectedClip = nullptr;
    if (!cuePath.empty()) {
        selectedClip = lib.FindByPath(cuePath);
    }
    if (!selectedClip && cueId != 0) {
        selectedClip = lib.FindById(cueId);
        if (selectedClip && cuePath.empty()) {
            data["CuePath"] = selectedClip->Path;
        }
    }
    std::string currentLabel = selectedClip ? selectedClip->Name : "None (drag audio here)";

    // Audio clip row + drag drop support like SpriteRenderer2D
    EditorUI::RenderStaticValueRow("Audio Clip", currentLabel, selectedClip == nullptr);

    // Drag-drop support
    HandleAssetDragDropTarget(kAudioExtensions, [&](const std::string& droppedPath) {
        const auto& clipInfo = lib.Register(droppedPath);
        data["CueId"] = clipInfo.Id;
        data["CuePath"] = clipInfo.Path;
        return true;
    }, [&](const std::string& rejectedPath) {
        QueueAssetDropError(rejectedPath, kAudioExtensions);
    });

    // Volume + Pitch sliders using EditorUI helpers
    EditorUI::RenderFloatRow("Volume", "", data, "Volume", 0.05f);
    EditorUI::RenderFloatRow("Pitch", "", data, "Pitch", 0.05f);

    // Checkboxes
    EditorUI::RenderCheckboxProperty("Loop", data, "Loop");
    EditorUI::RenderCheckboxProperty("Play On Start", data, "PlayOnStart");
    EditorUI::RenderCheckboxProperty("Spatial 3D", data, "Spatial3D");

    // Bus selection
    const char* busOptions[] = { "Master", "Music", "SFX", "UI", "Ambient" };
    int bus = data.value("Bus", 2);
    bus = std::clamp(bus, 0, 4);
    ImGui::Text("Bus");
    ImGui::SameLine();
    ImGui::SetCursorPosX(EditorUI::GetContentStartX() + ImGui::CalcTextSize("W").x + 6.0f);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##AudioBusCombo", busOptions[bus])) {
        for (int i = 0; i < 5; ++i) {
            bool selected = (bus == i);
            if (ImGui::Selectable(busOptions[i], selected)) {
                bus = i;
                data["Bus"] = bus;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Stereo pan (2D only)
    bool spatial3D = data.value("Spatial3D", true);
    if (!spatial3D) {
        EditorUI::RenderFloatRow("Pan", "", data, "Pan", 0.01f, -1.0f, 1.0f);
    }

    // Fade settings
    EditorUI::RenderCheckboxProperty("Enable Fade In", data, "EnableFadeIn");

    // Only show fade duration if fade is enabled
    bool fadeInEnabled = data.value("EnableFadeIn", false);
    if (fadeInEnabled) {
        EditorUI::RenderFloatRow("Fade In Duration", "s", data, "FadeInDuration", 0.1f);
    }

    EditorUI::RenderCheckboxProperty("Enable Fade Out", data, "EnableFadeOut");

    // Only show fade duration if fade is enabled
    bool fadeOutEnabled = data.value("EnableFadeOut", false);
    if (fadeOutEnabled) {
        EditorUI::RenderFloatRow("Fade Out Duration", "s", data, "FadeOutDuration", 0.1f);
    }

    // SINGLE EndPropertySection call
    EditorUI::EndPropertySection();
}

// Renders the Layer component properties
void ComponentUI::RenderLayer2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world) {
    (void)entity;
    (void)world;
    ImGuiIdScope id("Layer2D");
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
    ImGuiIdScope id("Material2D");

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
        // Inline thumbnail preview for material texture assignments
        if (EditorUI::PropertyFilterAllows(label)) {
            RenderInlineTexturePreview(data.value(idKey, 0u), "Material texture preview");
        }

        HandleAssetDragDropTarget(kImageExtensions, [&](const std::string& droppedPath) {
            auto tex = RM.Get<Texture>(droppedPath);
            if (tex) {
                data[idKey] = static_cast<uint32_t>(tex->ID());
                data[pathKey] = droppedPath;
                return true;
            }
            return false;
        }, [&](const std::string& rejectedPath) {
            QueueAssetDropError(rejectedPath, kImageExtensions);
        });
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
    static const std::vector<std::string> kMaterialFlagNames = BuildGenericFlagNames("Flag");
    EditorUI::RenderBitmaskDropdown("Flags", data, "Flags", kMaterialFlagNames, 0u);

    EditorUI::EndPropertySection();
}
