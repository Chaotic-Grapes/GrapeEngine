/* Start Header *****************************************************************/
/*!
\file    TilePalettePanel.cpp
\author  Samantha Leong (60%)
         Foo Rui Qin (40%)
\par     s.leong@digipen.edu
         ruiqin.foo@digipen.edu
\date    12th March 2026
\brief
Implements the TilePalettePanel editor UI responsible for:

- Displaying available tiles from loaded tilesets
- Allowing tile selection, rotation, and erasing
- Handling drag-and-drop asset loading
- Painting tiles into the active tilemap
- Editing per-tile collision masks
- Rendering hover previews in the editor viewport

This panel bridges editor UI interactions with runtime tilemap data,
ensuring visual edits are immediately reflected in rendering and persisted
to tilemap assets.

Dependencies include ImGui for UI and tilemap/tileset asset management.
*/
/* End Header *******************************************************************/

#include "TilePalettePanel.h"
#include "EditorStyle.h"

#include <imgui.h>
#include <algorithm>
#include <iostream>

#include "ecs/systems/RendererSystem.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "core/World/TileTypes.hpp"
#include "UndoSystem.h"
#include <cstring>

namespace {

    // Extracts the first asset path string from ImGui ASSET_PATHS payload data
    // Payload format can contain multiple null-terminated paths, first entry is enough for this panel
    std::string ParseFirstAssetPath(const ImGuiPayload* payload) {
        if (!payload || !payload->Data || payload->DataSize == 0) {
            return std::string();
        }

        const char* data = static_cast<const char*>(payload->Data);
        const char* end = data + payload->DataSize;
        if (data >= end) {
            return std::string();
        }

        // Payload is a sequence of null-terminated strings; take the first
        return std::string(data);
    }

    // Collision bitmask definitions for tile corners
    constexpr uint8_t kCollisionMaskBottomLeft = 1 << 0;  // Bit 0: Bottom-left corner
    constexpr uint8_t kCollisionMaskBottomRight = 1 << 1; // Bit 1: Bottom-right corner
    constexpr uint8_t kCollisionMaskTopLeft = 1 << 2;     // Bit 2: Top-left corner
    constexpr uint8_t kCollisionMaskTopRight = 1 << 3;    // Bit 3: Top-right corner
}

// Initializes tile palette context with initial tilemap, tileset list, world, and fonts
// Routes setup through SetEditingContext so all reset behavior stays centralized
void TilePalettePanel::Initialize(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset,
    ECS::World* world, ImFont* symbolsFont, ImFont* boldFont)
{
    m_world = world; // Store world pointer for physics syncing
    m_symbolsFont = symbolsFont;
    m_boldFont = boldFont;

    // Initialize editing context in one place so state resets stay consistent
    const std::vector<std::shared_ptr<Tileset>> tilesets = tileset ? std::vector<std::shared_ptr<Tileset>>{ tileset } 
        : std::vector<std::shared_ptr<Tileset>>{};

    const std::vector<std::string> paths;
    SetEditingContext(tileMap, tilesets, paths, 0, std::string(), glm::vec2(0.0f, 0.0f));
}

// Handles external asset-drop path forwarded from palette or viewport drag-drop targets
void TilePalettePanel::HandleAssetDrop(const std::string& assetPath)
{

    // Dropping a tilemap/tileset should not immediately arm viewport painting
    m_paintMode = false;

    if (m_assetDropCallback) {

        // Forward asset path to the editor-level handler
        m_assetDropCallback(assetPath);
    }
}

// Returns key used to cache tile-preview size per active tileset context
std::string TilePalettePanel::GetActiveTilesetPreviewKey() const
{

    // Use active tileset path as cache key so each tileset keeps its own preview-size preference
    if (m_activeTilesetIndex < m_tilesetPaths.size()) {
        return m_tilesetPaths[m_activeTilesetIndex];
    }

    // No active tileset means no cache key, caller falls back to default preview size
    return std::string();
}

// Refreshes tile-preview size from per-tileset cache and falls back to default when missing
void TilePalettePanel::RefreshTilePreviewSizeForActiveTileset()
{

    // Read cache key for current tileset context
    const std::string key = GetActiveTilesetPreviewKey();

    // Empty key means no valid tileset context, use baseline preview size
    if (key.empty()) {
        m_tilePreviewSize = 64.0f;
        return;
    }

    // Use cached size when present, else use baseline
    const auto it = m_tilePreviewSizeByTileset.find(key);
    m_tilePreviewSize = (it != m_tilePreviewSizeByTileset.end()) ? it->second : 64.0f;
}

// Clears per-tileset preview-size cache and resets active preview slider value
void TilePalettePanel::ClearTilePreviewSizeCache()
{
    m_tilePreviewSizeByTileset.clear();
    m_tilePreviewSize = 64.0f;
}

// Updates tilemap dropdown entries and selected tilemap id
void TilePalettePanel::SetTileMapList(const std::vector<TileMapListEntry>& entries, EntityId activeId)
{
    m_tileMapList = entries;                   // Replace the list of tilemaps for the dropdown
    m_activeTileMapId = activeId;              // Track which tilemap is currently active
    RefreshTilePreviewSizeForActiveTileset();  // Refresh preview size in case active tilemap context uses a different cached size
}

// Sets active tilemap and tileset editing context and resets transient paint state
void TilePalettePanel::SetEditingContext(const std::shared_ptr<TileMap>& tileMap,
    const std::vector<std::shared_ptr<Tileset>>& tilesets,
    const std::vector<std::string>& tilesetPaths,
    uint8_t activeTilesetIndex,
    const std::string& tileMapPath,
    const glm::vec2& worldOrigin)
{
    m_tileMap = tileMap; // Assign the new active tilemap
    m_tilesets = tilesets; // Assign the tileset list for this tilemap
    m_tilesetPaths = tilesetPaths; // Cache tileset paths for UI labels
    m_activeTilesetIndex = activeTilesetIndex; // Track active tileset index for packing

    if (m_activeTilesetIndex >= m_tilesets.size()) {
        m_activeTilesetIndex = 0;
    }
    m_tileset = m_tilesets.empty() ? nullptr : m_tilesets[m_activeTilesetIndex]; // Assign active tileset
    m_tileMapPath = tileMapPath; // Track map path for auto-save on edits
    m_worldOrigin = worldOrigin; // Cache the tilemap origin in world space
    m_selectedTileID = 0; // Reset tile selection when swapping tilesets
    m_currentRotation = 0; // Reset rotation to default
    m_isEraser = false; // Reset eraser toggle
    m_collisionHasLastPaint = false; // Reset collision drag tracking
    m_collisionLastPaintKey = 0;
    m_collisionLastPaintMask = 0;

    RefreshTilePreviewSizeForActiveTileset();  // Refresh preview size cache for newly active tileset context
}

// Toggles collision-edit mode and manages paint-mode handoff state
void TilePalettePanel::SetCollisionEditActive(bool active)
{
    if (active == m_collisionEditActive) return;

    // Reset drag-paint trackers on mode transitions so first stroke starts cleanly
    m_collisionEditActive = active;
    m_collisionHasLastPaint = false;
    m_collisionLastPaintKey = 0;
    m_collisionLastPaintMask = 0;

    // Entering collision mode temporarily disables tile paint mode and prepares collision brush defaults
    if (active) {
        m_collisionPrevPaintMode = m_paintMode;
        m_paintMode = false;
        m_collisionEraser = false;

        if (m_collisionBrushMask == 0) m_collisionBrushMask = 0x0F;  // Default to all corners if no brush mask set
    }

    // Leaving collision mode restores previous tile-paint mode state
    else {
        m_paintMode = m_collisionPrevPaintMode;
    }
}

// Renders tile palette UI for tilemap selection, tileset tools, tile preview grid, and collision tools
void TilePalettePanel::Render()
{
    if (!m_active) return;

    if (ImGui::Begin("Tile Palette"))
    {
        // Build a full panel drop target so users can drop map or tileset assets anywhere in this window

        const ImVec2 contentMinLocal = ImGui::GetWindowContentRegionMin(); // Minimum content corner in local window space
        const ImVec2 contentMaxLocal = ImGui::GetWindowContentRegionMax(); // Maximum content corner in local window space
        const ImVec2 contentSize(contentMaxLocal.x - contentMinLocal.x, contentMaxLocal.y - contentMinLocal.y);

        // Only create the invisible drop target when region size is valid
        if (contentSize.x > 0.0f && contentSize.y > 0.0f) {
            // Save and restore cursor so this invisible widget does not shift normal layout
            const ImVec2 cursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(contentMinLocal);
            ImGui::SetNextItemAllowOverlap(); // Let visible widgets overlap this target and remain interactive
            ImGui::InvisibleButton("##TilePaletteDropTarget", contentSize);

            // Accept dropped asset path payload and forward first path to panel callback chain
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
                    const std::string assetPath = ParseFirstAssetPath(payload);

                    // Ignore malformed payloads that decode to an empty path
                    if (!assetPath.empty()) {
                        HandleAssetDrop(assetPath); // Editor decides whether this path becomes active tilemap or tileset
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SetCursorPos(cursorPos);
        }

        // Tilemap combo determines which map entity receives paint and collision edits
        if (!m_tileMapList.empty()) {
            // Resolve current combo label from active id
            const char* currentLabel = "None";
            for (const auto& entry : m_tileMapList) {
                if (entry.Id == m_activeTileMapId) {
                    currentLabel = entry.Name.c_str();
                    break;
                }
            }

            ImGui::Text("Active Tilemap");
            ImGui::SameLine();

            // Combo lists all available tilemaps and notifies owner when selection changes
            if (ImGui::BeginCombo("##ActiveTilemap", currentLabel)) {
                for (const auto& entry : m_tileMapList) {
                    const bool isSelected = (entry.Id == m_activeTileMapId);
                    if (ImGui::Selectable(entry.Name.c_str(), isSelected)) {
                        m_activeTileMapId = entry.Id;

                        // Callback switches external editing context to the selected tilemap
                        if (m_activeTileMapCallback) {
                            m_activeTileMapCallback(entry.Id);
                        }
                    }

                    // Keep keyboard focus on currently active entry for quick Enter confirmation
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
        }

        // Tileset combo picks which atlas is shown in palette and used for tile packing
        if (!m_tilesetPaths.empty()) {
            std::string tilesetLabel = "None";
            if (m_activeTilesetIndex < m_tilesetPaths.size()) {
                const std::string& fullPath = m_tilesetPaths[m_activeTilesetIndex];
                const size_t slash = fullPath.find_last_of("\\/");

                // Use filename only to keep combo labels compact
                tilesetLabel = (slash == std::string::npos) ? fullPath : fullPath.substr(slash + 1);
            }

            ImGui::Text("Active Tileset");
            ImGui::SameLine();

            // Combo rows are generated from cached asset paths
            if (ImGui::BeginCombo("##ActiveTileset", tilesetLabel.c_str())) {
                for (size_t i = 0; i < m_tilesetPaths.size(); ++i) {
                    const bool isSelected = (i == m_activeTilesetIndex);
                    const std::string& fullPath = m_tilesetPaths[i];
                    const size_t slash = fullPath.find_last_of("\\/");
                    const std::string displayName = (slash == std::string::npos) ? fullPath : fullPath.substr(slash + 1);

                    if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                        m_activeTilesetIndex = static_cast<uint8_t>(i);
                        m_tileset = (i < m_tilesets.size()) ? m_tilesets[i] : nullptr;

                        // Restore this tileset own zoom cache so visual scale is remembered per atlas
                        RefreshTilePreviewSizeForActiveTileset();

                        // Clear tile selection because tile ids belong to the selected tileset
                        m_selectedTileID = 0;

                        // Notify owner to sync any external tileset dependent state
                        if (m_activeTilesetCallback) {
                            m_activeTilesetCallback(m_activeTilesetIndex);
                        }
                    }

                    // Default keyboard focus to active row inside combo popup
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
        }

        if (!m_tileset)
        {
            // Empty state prompt explains why no tile buttons are currently visible
            ImGui::TextDisabled("Drag and drop an asset here");
        }
        else
        {
            // Keep paint mode toggle disabled while collision editor is active to avoid conflicting tools
            if (m_collisionEditActive) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button(m_paintMode ? "Paint Mode: ON" : "Paint Mode: OFF")) {
                if (!m_collisionEditActive) {
                    m_paintMode = !m_paintMode;
                }
            }

            // Explain disabled state when users hover paint toggle during collision mode
            if (m_collisionEditActive) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Disabled while collision editing");
                }
            }

            // Tile eraser only affects tile placement tool state
            ImGui::SameLine();
            if (ImGui::Button(m_isEraser ? "Eraser [ON]" : "Eraser"))
            {
                m_isEraser = !m_isEraser;
            }

            // Rotation is stored as quarter turns and displayed as degrees for readability
            ImGui::SameLine();
            ImGui::Text("Rotation: %d deg", m_currentRotation * 90);
            ImGui::SameLine();
            if (ImGui::Button("Rotate (R)"))
            {
                m_currentRotation = (m_currentRotation + 1) % 4;
            }

            // Preview slider controls thumbnail button size in the tile palette grid
            ImGui::SliderFloat("Tile Preview Size", &m_tilePreviewSize, kTilePreviewMin, kTilePreviewMax, "%.0f px");

            // Clamp after slider interaction so stored values always stay inside supported range
            m_tilePreviewSize = std::clamp(m_tilePreviewSize, kTilePreviewMin, kTilePreviewMax);

            // Cache this zoom value under active tileset key so each tileset remembers own zoom
            const std::string previewKey = GetActiveTilesetPreviewKey();

            // Empty key means no active tileset context to attach a cache entry to
            if (!previewKey.empty()) {
                m_tilePreviewSizeByTileset[previewKey] = m_tilePreviewSize;
            }

            ImGui::Separator();

            // Compute content right edge so tile buttons can wrap to next row when needed
            float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

            // Read tile definitions from currently active tileset atlas
            const auto& tiles = m_tileset->GetTiles();

            ImGuiStyle& style = ImGui::GetStyle();
            const float buttonSize = m_tilePreviewSize;

            // Render one image button per tile definition
            int i = 0;
            for (const auto& [id, def] : tiles)
            {
                // Scope all ImGui ids and styles per tile so each button stays independent
                ImGui::PushID(id);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));        // Keep tile button background fully transparent
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0)); // Keep hover fill transparent so texture stays visible
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));  // Keep active fill transparent and use border as state cue

                // Convert UV vertical direction because atlas UVs are OpenGL style and ImGui is top origin
                // Using (u0, v1) to (u1, v0) displays the correct tile orientation inside ImGui image widgets
                ImVec2 imUV0(def.uv.u0, def.uv.v1);
                ImVec2 imUV1(def.uv.u1, def.uv.v0);

                // Build unique string id so ImageButton labels stay hidden while ids stay stable
                std::string strId = "Tile_" + std::to_string(id);

                // Choosing a tile arms paint mode and exits eraser so placement starts immediately
                if (ImGui::ImageButton(strId.c_str(), (ImTextureID)(uintptr_t)m_tileset->GetTextureId(), ImVec2(buttonSize, buttonSize), imUV0, imUV1))
                {
                    m_selectedTileID = id;
                    m_isEraser = false;
                    m_paintMode = true;
                }

                // Hover and selection states drive custom outline colors
                const bool isHovered = ImGui::IsItemHovered();
                const bool isSelected = (m_selectedTileID == id && !m_isEraser);

                if (isHovered || isSelected) {
                    // Draw explicit border so state remains visible even with transparent button style
                    const ImU32 borderColor = isSelected
                        ? ImGui::GetColorU32(ImVec4(1.0f, 0.85f, 0.15f, 1.0f))
                        : ImGui::GetColorU32(ImVec4(0.65f, 0.75f, 0.95f, 0.85f));

                    const ImVec2 min = ImGui::GetItemRectMin();
                    const ImVec2 max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(min, max, borderColor, 0.0f, 0, 2.0f);
                }

                // Continue row when next tile still fits before visible right boundary
                float lastButtonX = ImGui::GetItemRectMax().x;
                float nextButtonX = lastButtonX + style.ItemSpacing.x + buttonSize;

                if (i + 1 < tiles.size() && nextButtonX < windowVisibleX) {
                    ImGui::SameLine();
                }

                ImGui::PopStyleColor(3);
                ImGui::PopID();
                i++;
            }

            // Keyboard shortcut mirrors rotate button while palette window is focused
            if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_R))
            {
                m_currentRotation = (m_currentRotation + 1) % 4;
            }
        }

        // Collision section is rendered only when there is an active tilemap to edit
        if (m_tileMap) {
            ImGui::BeginChild("##CollisionEditSection", ImVec2(0.0f, 0.0f), true);

            // Gather capability flags once so controls can share the same enable conditions
            const bool hasTileMap = (m_tileMap != nullptr);
            const bool hasTileset = (m_tileset != nullptr);

            // Clicking anywhere in this child activates collision mode when it is currently inactive
            if (!m_collisionEditActive && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                SetCollisionEditActive(true);
            }

            const bool collisionToolsEnabled = m_collisionEditActive && hasTileMap && hasTileset;
            const bool sectionDisabled = !m_collisionEditActive;

            // Dim the section when not active but keep controls visible for discoverability
            if (sectionDisabled) {
                ImGui::BeginDisabled();
            }

            // Title and hint explain how to apply collision paint inside viewport
            if (m_boldFont) {
                ImGui::PushFont(m_boldFont);
            }
            ImGui::Text("Collision Edit");
            if (m_boldFont) {
                ImGui::PopFont();
            }
            ImGui::TextDisabled("LMB: paint collision");

            // Exit button allows explicit mode leave without requiring outside panel click
            if (ImGui::Button("Exit")) {
                SetCollisionEditActive(false);
            }
            ImGui::SameLine();

            // Collision eraser state is separate so switching collision tool does not touch tile paint tool
            const char* eraserLabel = m_collisionEraser ? "Collision Eraser [ON]" : "Collision Eraser";
            if (!collisionToolsEnabled) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button(eraserLabel)) {
                m_collisionEraser = !m_collisionEraser;
            }
            if (!collisionToolsEnabled) {
                ImGui::EndDisabled();
            }
            ImGui::Separator();

            // Brush mask UI maps directly to tile corner bits used by collision storage
            ImGui::Text("Brush");

            // Brush cell size follows current frame height so the 2x2 block matches standard control scale
            const float cellSize = ImGui::GetFrameHeight();

            auto drawBrushCell = [&](const char* id, uint8_t bit) {
                // Active state is true when this corner bit is set in current brush mask
                const bool active = (m_collisionBrushMask & bit) != 0;

                // Active corners use success colors and inactive corners use secondary colors
                ImGui::PushStyleColor(ImGuiCol_Button, active ? EditorStyle::SuccessButton : EditorStyle::SecondaryButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? EditorStyle::SuccessButtonHover : EditorStyle::SecondaryButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? EditorStyle::SuccessButtonActive : EditorStyle::SecondaryButtonActive);

                // Clicking a corner toggles only that bit in mask and exits eraser mode
                if (ImGui::Button(id, ImVec2(cellSize, cellSize))) {
                    m_collisionEraser = false;
                    if (active) {
                        // Remove this corner from brush output

                        m_collisionBrushMask = static_cast<uint8_t>(m_collisionBrushMask & ~bit);
                    }
                    else {
                        // Add this corner to brush output

                        m_collisionBrushMask = static_cast<uint8_t>(m_collisionBrushMask | bit);
                    }
                }
                ImGui::PopStyleColor(3);
            };

            // Lock mask editing when collision tools are not available
            if (!collisionToolsEnabled) {
                ImGui::BeginDisabled();
            }

            // Grid placement mirrors physical corners of a tile
            ImGui::PushID("CollisionBrushGrid");
            drawBrushCell("##TL", kCollisionMaskTopLeft);     // Top left
            ImGui::SameLine();

            drawBrushCell("##TR", kCollisionMaskTopRight);    // Top right

            drawBrushCell("##BL", kCollisionMaskBottomLeft);  // Bottom left
            ImGui::SameLine();

            drawBrushCell("##BR", kCollisionMaskBottomRight); // Bottom right
            ImGui::PopID();
            ImGui::Separator();

            // Presets provide one click masks for common collision patterns
            ImGui::Text("Presets");
            const struct { const char* label; const char* tooltip; uint8_t mask; } presets[] = {
                {"F", "Full", 0x0F},
                {"E", "Empty", 0x00},
                {"T", "Top", 0x0C},
                {"B", "Bottom", 0x03},
                {"L", "Left", 0x05},
                {"R", "Right", 0x0A}
            };

            // Render compact preset row and apply selected mask immediately
            constexpr size_t presetCount = sizeof(presets) / sizeof(presets[0]);
            for (size_t i = 0; i < presetCount; i++) {
                if (i > 0) {
                    ImGui::SameLine();
                }

                // Preset click also exits eraser so mask paint can be applied directly
                if (ImGui::Button(presets[i].label)) {
                    m_collisionEraser = false;
                    m_collisionBrushMask = presets[i].mask;
                }

                // Tooltip expands short label into full meaning
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(presets[i].tooltip);
                }
            }
            if (!collisionToolsEnabled) {
                ImGui::EndDisabled();
            }

            // Close section disabled scope opened when collision mode is inactive
            if (sectionDisabled) {
                ImGui::EndDisabled();
            }

            ImGui::EndChild();
        }

    }
    ImGui::End();
}

// Draws tile placement preview at viewport hover position using current tile, rotation, and paint mode
void TilePalettePanel::OnViewportHover(const glm::vec2& worldPos)
{
    if (!m_tileMap || !m_tileset) return;

    // Handle 'R' key globally when hovering viewport
    if (ImGui::IsKeyPressed(ImGuiKey_R))
    {
        m_currentRotation = (m_currentRotation + 1) % 4;
    }

    const glm::vec2 localPos = worldPos - m_worldOrigin; // Convert world space to tilemap-local space

    // Compute signed tile indices so negative coordinates are supported
    const int32_t tx = m_tileMap->WorldToTileSigned(localPos.x); // Signed tile coordinate in map space
    const int32_t ty = m_tileMap->WorldToTileSigned(localPos.y); // Signed tile coordinate in map space

    // Calculate tile world-space bounds for rendering preview overlay
    float tileSize = m_tileMap->TileSize();
    glm::vec2 tilePos(m_tileMap->TileToWorldSigned(tx), m_tileMap->TileToWorldSigned(ty)); // Tile origin in local space
    tilePos += m_worldOrigin; // Shift tile origin into world space
    glm::vec2 size(tileSize, tileSize);

    ECS::RendererSystem* rs = ECS::RendererSystem::GetInstance();
    if (rs) {
        glm::vec2 min(tilePos.x, tilePos.y);
        glm::vec2 max(tilePos.x + size.x, tilePos.y + size.y);
        if (m_isEraser) {
            glm::vec4 color(1.0f, 0.0f, 0.0f, 0.35f); // Red translucent preview for eraser
            rs->SubmitFilledQuad(min, max, color);
            return;
        }

        TileUV uv;
        if (!m_tileset->GetTileUV(m_selectedTileID, uv)) {
            glm::vec4 color(1.0f, 1.0f, 1.0f, 0.25f); // Fallback to a light fill if UV is missing
            rs->SubmitFilledQuad(min, max, color);
            return;
        }

        const glm::vec4 uvRect(uv.u0, uv.v0, uv.u1, uv.v1); // Pack UVs for submitQuad
        const glm::vec2 center(tilePos.x + size.x * 0.5f, tilePos.y + size.y * 0.5f);
        const glm::vec4 tint(1.0f, 1.0f, 1.0f, 0.5f); // Translucent tile preview
        const float rotation = static_cast<float>(m_currentRotation) * 1.57079632679f;
        rs->SubmitOverlayQuad(center, size, m_tileset->GetTextureId(), uvRect, tint, rotation);
    }
}

// Draws collision-mask preview overlay for the currently hovered tile while in collision edit mode
void TilePalettePanel::OnViewportCollisionHover(const glm::vec2& worldPos)
{
    // Only draw collision preview when collision tools are active and resources are valid
    if (!m_collisionEditActive || !m_tileMap || !m_tileset) return;

    // Grab renderer once and bail if debug-overlay submission is unavailable
    ECS::RendererSystem* rs = ECS::RendererSystem::GetInstance();
    if (!rs) return;

    // Convert hover world position into signed tile coordinates
    const glm::vec2 localPos = worldPos - m_worldOrigin;
    const int32_t tx = m_tileMap->WorldToTileSigned(localPos.x);
    const int32_t ty = m_tileMap->WorldToTileSigned(localPos.y);

    // Check if it's a valid tile
    if (m_tileMap->GetTileSigned(0, tx, ty) == EMPTY_TILE) {
        return;
    }

    // Draw a preview of the collision brush on that tile
    const float tileSize = m_tileMap->TileSize();
    if (tileSize <= 0.0f) return;

    // Compute world-space tile origin used as base for quadrant overlay drawing
    const glm::vec2 tileWorld(m_worldOrigin.x + m_tileMap->TileToWorldSigned(tx), 
        m_worldOrigin.y + m_tileMap->TileToWorldSigned(ty));

    // Quadrant size is half tile size because mask stores per-corner occupancy
    const float subSize = tileSize * 0.5f;

    // Eraser preview overlays full tile in red because erase clears all mask bits for this tile
    if (m_collisionEraser) {
        const glm::vec4 eraseColor(0.9f, 0.2f, 0.2f, 0.25f);
        rs->SubmitFilledQuad(tileWorld, tileWorld + glm::vec2(tileSize, tileSize), eraseColor);
        return;
    }

    // Lower four bits encode bottom-left, bottom-right, top-left, top-right quadrants
    const uint8_t mask = static_cast<uint8_t>(m_collisionBrushMask & 0x0F);
    if (mask == 0) return;

    // Draw green translucent fill for each active corner bit to preview final painted mask
    const glm::vec4 previewColor(0.2f, 0.85f, 0.35f, 0.28f);
    if (mask & kCollisionMaskBottomLeft) {
        rs->SubmitFilledQuad(tileWorld, tileWorld + glm::vec2(subSize, subSize), previewColor);
    }
    if (mask & kCollisionMaskBottomRight) {
        rs->SubmitFilledQuad(tileWorld + glm::vec2(subSize, 0.0f),
            tileWorld + glm::vec2(tileSize, subSize), previewColor);
    }
    if (mask & kCollisionMaskTopLeft) {
        rs->SubmitFilledQuad(tileWorld + glm::vec2(0.0f, subSize),
            tileWorld + glm::vec2(subSize, tileSize), previewColor);
    }
    if (mask & kCollisionMaskTopRight) {
        rs->SubmitFilledQuad(tileWorld + glm::vec2(subSize, subSize),
            tileWorld + glm::vec2(tileSize, tileSize), previewColor);
    }
}

// Applies tile paint or erase at viewport click position with undo and autosave integration
bool TilePalettePanel::OnViewportClick(const glm::vec2& worldPos, bool isRightClick)
{
    if (!m_tileMap || !m_tileset) return false;

    const glm::vec2 localPos = worldPos - m_worldOrigin;         // Convert world space to tilemap-local space
    const int32_t tx = m_tileMap->WorldToTileSigned(localPos.x); // Signed tile coordinate in map space
    const int32_t ty = m_tileMap->WorldToTileSigned(localPos.y); // Signed tile coordinate in map space
    const int64_t key = PackCoord(tx, ty);

    // Tile edits target layer 0 and can trigger expansion near borders
    if (m_tileMap->LayerCount() == 0) return false;
    const uint32_t kExpandMargin = 2; // Expand when painting within N tiles of the edge
    const uint32_t kExpandStep = 16; // Grow by this many tiles per expansion

    m_tileMap->ExpandLayerToFit(0, tx, ty, kExpandMargin, kExpandStep); // Grow the layer to fit signed coords
    if (!m_tileMap->IsTileInBounds(tx, ty)) return false; // Bail out if still out of bounds

    bool erasing = isRightClick || m_isEraser;
    if (m_hasLastPaint && key == m_lastPaintKey && erasing == m_lastPaintErase) {
        return false; // Skip redundant paint when dragging over the same tile
    }
    
    // Read current tile before edit so no-op and undo logic can compare old versus new
    TileID oldTile = m_tileMap->GetTileSigned(0, tx, ty);

    // Build replacement tile value from tool state
    TileID newTile = EMPTY_TILE;

    // Packing stores selected tile id, rotation, and tileset index in one TileID
    if (!erasing) {
        newTile = PackTile(m_selectedTileID, m_currentRotation, m_activeTilesetIndex);
    }

    // Skip writes and undo commands when value is unchanged
    if (oldTile == newTile) {
        return false;
    }

    // Shared callback centralizes scene-dirty notification and tilemap persistence
    auto onTileChanged = [this](int32_t, int32_t, TileID) {

        // Mark the scene dirty so saves are enabled and tracked
        Messaging::MessageSystem::Notify(Messaging::SceneModified("Tilemap paint"));

        // Auto-save tilemap file when path is available
        if (!m_tileMapPath.empty()) {

            // Persist edits immediately so the tilemap asset stays in sync
            m_tileMap->SaveMap(m_tileMapPath);
        }
    };

    // Route edits through undo stack when available
    if (m_undoSystem) {

        // Command captures old and new values to support undo and redo
        auto command = std::make_unique<Editor::TilePaintCommand>(
            m_tileMap, tx, ty, oldTile, newTile, onTileChanged
        );

        // Execute through undo system so action is tracked in editor history
        m_undoSystem->ExecuteCommand(std::move(command));
    }

    // Fallback direct-write path when undo stack is unavailable
    else {
        m_tileMap->SetTileSigned(0, tx, ty, newTile);
        onTileChanged(tx, ty, newTile);
    }

    m_hasLastPaint = true;
    m_lastPaintKey = key;
    m_lastPaintErase = erasing;
    return true;
}

// Applies collision mask edit at viewport click position with undo and autosave integration
bool TilePalettePanel::OnViewportCollisionClick(const glm::vec2& worldPos)
{

    // Collision edits require active collision mode plus valid tilemap and tileset state
    if (!m_collisionEditActive || !m_tileMap || !m_tileset) return false;
    if (m_tileMap->LayerCount() == 0) return false;

    // Convert world space to tilemap-local space
    const glm::vec2 localPos = worldPos - m_worldOrigin; 

    // Signed coordinates allow painting maps with negative-space world origins
    // then pack into one key so drag-paint dedup can compare last painted tile quickly
    const int32_t tx = m_tileMap->WorldToTileSigned(localPos.x);
    const int32_t ty = m_tileMap->WorldToTileSigned(localPos.y);
    const int64_t key = PackCoord(tx, ty);

    // Collision is only meaningful on occupied tiles
    if (m_tileMap->GetTileSigned(0, tx, ty) == EMPTY_TILE) {
        return false;
    }

    // Ensure layer has capacity near borders before writing mask bits
    const uint32_t kExpandMargin = 2;  // Expand when painting within N tiles of the edge
    const uint32_t kExpandStep = 16;   // Grow by this many tiles per expansion

    // Expand as needed so collision edits remain valid near negative space and map edges
    m_tileMap->ExpandLayerToFit(0, tx, ty, kExpandMargin, kExpandStep);
    if (!m_tileMap->IsTileInBounds(tx, ty)) return false;

    // Eraser clears mask to zero, brush mode applies current lower 4 brush bits
    const uint8_t newMask = static_cast<uint8_t>(m_collisionEraser ? 0 : (m_collisionBrushMask & 0x0F));

    // Skip redundant paint when dragging over the same tile
    if (m_collisionHasLastPaint && key == m_collisionLastPaintKey && newMask == m_collisionLastPaintMask) {
        return false; 
    }

    // Skip writes when resulting mask matches current value
    const uint8_t oldMask = m_tileMap->GetCollisionMaskSigned(tx, ty);
    if (oldMask == newMask) {
        return false;
    }

    // Shared callback notifies dirty state and persists collision edits when path is available
    auto onCollisionChanged = [this](int32_t, int32_t, uint8_t) {
        Messaging::MessageSystem::Notify(Messaging::SceneModified("Tilemap collision"));
        if (!m_tileMapPath.empty()) {
            m_tileMap->SaveMap(m_tileMapPath);
        }
    };

    // Route through undo system so collision paints can be undone and redone
    if (m_undoSystem) {
        auto command = std::make_unique<Editor::TileCollisionPaintCommand>(
            m_tileMap, tx, ty, oldMask, newMask, onCollisionChanged
        );
        m_undoSystem->ExecuteCommand(std::move(command));
    }
    else {

        // No undo system available, apply the mask directly
        m_tileMap->SetCollisionMaskSigned(tx, ty, newMask);
        onCollisionChanged(tx, ty, newMask);
    }

    // Save last painted state to suppress duplicate drag writes on same tile/mask pair
    m_collisionHasLastPaint = true;
    m_collisionLastPaintKey = key;
    m_collisionLastPaintMask = newMask;
    return true;
}
