/* Start Header *****************************************************************/
/*!
\file    TilePalettePanel.cpp
\author Samantha Leong 
\par    s.leong@digipen.edu
\date   3rd February 2026
\brief
Implements the TilePalettePanel editor UI responsible for:

- Displaying available tiles from loaded tilesets
- Allowing tile selection, rotation, and erasing
- Handling drag-and-drop asset loading
- Painting tiles into the active tilemap
- Synchronizing tile collisions with ECS physics entities
- Rendering hover previews in the editor viewport

This panel bridges editor UI interactions with runtime tilemap data,
ensuring visual edits are immediately reflected in both rendering and
physics systems.

Dependencies include ImGui for UI, ECS world systems for physics,
and tilemap/tileset asset management.

*/
/* End Header *******************************************************************/


#include "TilePalettePanel.h"
#include "EditorStyle.h"

#include <imgui.h>
#include <algorithm>
#include <iostream>

#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "core/World/TileTypes.hpp"
#include "UndoSystem.h"
#include <cstring>

// Assuming ImGui::ImageButton takes ImTextureID (void*)
// Tileset::GetTextureId() returns uint32_t (OpenGL ID).
// We cast it to ImTextureID.

namespace {
    /*!
    \brief Extracts the first asset path from an ImGui drag-drop payload.

    The payload format contains multiple null-terminated strings. This
    function safely reads the first string and returns it.

    \param payload Drag-drop payload from ImGui
    \return First asset path string, or empty string if invalid
    */
    // Extract the first path from the ASSET_PATHS payload format.
    std::string ParseFirstAssetPath(const ImGuiPayload* payload) {
        if (!payload || !payload->Data || payload->DataSize == 0) {
            return std::string();
        }

        const char* data = static_cast<const char*>(payload->Data);
        const char* end = data + payload->DataSize;
        if (data >= end) {
            return std::string();
        }

        // Payload is a sequence of null-terminated strings; take the first.
        return std::string(data);
    }

	// Collision bitmask definitions for tile corners
	constexpr uint8_t kCollisionMaskBottomLeft = 1 << 0;  // Bit 0: Bottom-left corner
	constexpr uint8_t kCollisionMaskBottomRight = 1 << 1; // Bit 1: Bottom-right corner
	constexpr uint8_t kCollisionMaskTopLeft = 1 << 2;     // Bit 2: Top-left corner
	constexpr uint8_t kCollisionMaskTopRight = 1 << 3;    // Bit 3: Top-right corner
}

/*------------------------------------------------------------------*/
/*!
\brief Initializes the TilePalettePanel editing environment.

This function establishes the initial editor context by:

- Storing a pointer to the ECS world for physics synchronization
- Preparing a tileset collection (if provided)
- Resetting editor state via SetEditingContext()

The initialization funnels all setup through SetEditingContext()
to ensure consistent state resets and avoid duplicated logic.

\param tileMap  Tilemap to edit
\param tileset  Initial tileset used for palette display
\param world    ECS world used for spawning/removing physics entities
*/
void TilePalettePanel::Initialize(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset,
    ECS::World* world, ImFont* symbolsFont, ImFont* boldFont)
{
    m_world = world; // Store world pointer for physics syncing.
    m_symbolsFont = symbolsFont;
    m_boldFont = boldFont;
    // Initialize editing context in one place so state resets stay consistent.
    const std::vector<std::shared_ptr<Tileset>> tilesets = tileset ? std::vector<std::shared_ptr<Tileset>>{ tileset } : std::vector<std::shared_ptr<Tileset>>{};
    const std::vector<std::string> paths;
    SetEditingContext(tileMap, tilesets, paths, 0, std::string(), glm::vec2(0.0f, 0.0f));
}

/*------------------------------------------------------------------*/
/*!
\brief Handles asset drop events forwarded from the UI.

This delegates asset handling to the editor-level callback so that
tilemaps or tilesets can be resolved externally.

\param assetPath Path of dropped asset
*/
void TilePalettePanel::HandleAssetDrop(const std::string& assetPath)
{
    if (m_assetDropCallback) {
        // Forward asset path to the editor-level handler.
        m_assetDropCallback(assetPath);
    }
}

/*------------------------------------------------------------------*/
/*!
\brief Updates the list of tilemaps available for editing.

\param entries Tilemap dropdown entries
\param activeId Currently active tilemap ID
*/
void TilePalettePanel::SetTileMapList(const std::vector<TileMapListEntry>& entries, EntityId activeId)
{
    m_tileMapList = entries; // Replace the list of tilemaps for the dropdown.
    m_activeTileMapId = activeId; // Track which tilemap is currently active.
}

/*------------------------------------------------------------------*/
/*!
\brief Sets the active editing context.

This resets physics entities from previous maps, switches tilesets,
resets selection state, and updates editor metadata.

\param tileMap Active tilemap
\param tilesets Available tilesets
\param tilesetPaths UI labels for tilesets
\param activeTilesetIndex Active tileset index
\param tileMapPath Asset path for saving
\param worldOrigin Tilemap origin in world space
*/
void TilePalettePanel::SetEditingContext(const std::shared_ptr<TileMap>& tileMap,
    const std::vector<std::shared_ptr<Tileset>>& tilesets,
    const std::vector<std::string>& tilesetPaths,
    uint8_t activeTilesetIndex,
    const std::string& tileMapPath,
    const glm::vec2& worldOrigin)
{
    // Remove any cached physics entities tied to the previous map.
    if (m_world) {
        for (const auto& [key, entity] : m_physicsEntities) {
            (void)key; // Suppress unused warning for the map key.
            if (m_world->IsAlive(entity)) {
                m_world->Destroy(entity); // Clean up physics entities spawned by the old map.
            }
        }
    }

    m_physicsEntities.clear(); // Reset cached physics entity map.
    m_tileMap = tileMap; // Assign the new active tilemap.
    m_tilesets = tilesets; // Assign the tileset list for this tilemap.
    m_tilesetPaths = tilesetPaths; // Cache tileset paths for UI labels.
    m_activeTilesetIndex = activeTilesetIndex; // Track active tileset index for packing.
    if (m_activeTilesetIndex >= m_tilesets.size()) {
        m_activeTilesetIndex = 0;
    }
    m_tileset = m_tilesets.empty() ? nullptr : m_tilesets[m_activeTilesetIndex]; // Assign active tileset.
    m_tileMapPath = tileMapPath; // Track map path for auto-save on edits.
    m_worldOrigin = worldOrigin; // Cache the tilemap origin in world space.
    m_selectedTileID = 0; // Reset tile selection when swapping tilesets.
    m_currentRotation = 0; // Reset rotation to default.
    m_isEraser = false; // Reset eraser toggle.
    m_collisionHasLastPaint = false; // Reset collision drag tracking.
    m_collisionLastPaintKey = 0;
    m_collisionLastPaintMask = 0;
}

/*------------------------------------------------------------------*/
/*!
\brief Enables or disables collision edit mode.
*/
void TilePalettePanel::SetCollisionEditActive(bool active)
{
    if (active == m_collisionEditActive) return;

	// When toggling collision edit mode, we need to reset paint tracking state to avoid issues with drag painting
    m_collisionEditActive = active;
    m_collisionHasLastPaint = false;
    m_collisionLastPaintKey = 0;
    m_collisionLastPaintMask = 0;

	// When activating collision edit mode, we want to disable normal paint mode and set up for collision painting
    if (active) {
        m_collisionPrevPaintMode = m_paintMode;
        m_paintMode = false;
        m_collisionEraser = false;
		if (m_collisionBrushMask == 0) m_collisionBrushMask = 0x0F;  // Default to all corners if no brush mask set
    } 
    // When deactivating, we restore the previous paint mode state
    else {
        m_paintMode = m_collisionPrevPaintMode;
    }
}

/*------------------------------------------------------------------*/
/*!
\brief Renders the ImGui tile palette window.

Provides:

- Drag-drop asset handling
- Tilemap & tileset selection
- Tile grid display
- Tile inspector
- Rotation/eraser tools

All editor interactions are processed here.
*/
void TilePalettePanel::Render()
{
    if (!m_active) return;

    if (ImGui::Begin("Tile Palette"))
    {
        // Allow drag-and-drop of assets directly onto the palette window.
        const ImVec2 contentMinLocal = ImGui::GetWindowContentRegionMin(); // Content region min (local).
        const ImVec2 contentMaxLocal = ImGui::GetWindowContentRegionMax(); // Content region max (local).
        const ImVec2 contentSize(contentMaxLocal.x - contentMinLocal.x, contentMaxLocal.y - contentMinLocal.y);
        if (contentSize.x > 0.0f && contentSize.y > 0.0f) {
            const ImVec2 cursorPos = ImGui::GetCursorPos(); // Cache cursor in local coordinates.
            ImGui::SetCursorPos(contentMinLocal);
            ImGui::SetNextItemAllowOverlap(); // Keep the invisible target from blocking palette interaction.
            ImGui::InvisibleButton("##TilePaletteDropTarget", contentSize);
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
                    const std::string assetPath = ParseFirstAssetPath(payload);
                    if (!assetPath.empty()) {
                        HandleAssetDrop(assetPath); // Let the editor resolve the tileset + tilemap.
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SetCursorPos(cursorPos);
        }

        if (!m_tileMapList.empty()) {
            const char* currentLabel = "None";
            for (const auto& entry : m_tileMapList) {
                if (entry.Id == m_activeTileMapId) {
                    currentLabel = entry.Name.c_str();
                    break;
                }
            }

            ImGui::Text("Active Tilemap");
            ImGui::SameLine();
            if (ImGui::BeginCombo("##ActiveTilemap", currentLabel)) {
                for (const auto& entry : m_tileMapList) {
                    const bool isSelected = (entry.Id == m_activeTileMapId);
                    if (ImGui::Selectable(entry.Name.c_str(), isSelected)) {
                        m_activeTileMapId = entry.Id;
                        if (m_activeTileMapCallback) {
                            m_activeTileMapCallback(entry.Id); // Let the editor switch the active tilemap.
                        }
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
        }

        if (!m_tilesetPaths.empty()) {
            std::string tilesetLabel = "None";
            if (m_activeTilesetIndex < m_tilesetPaths.size()) {
                const std::string& fullPath = m_tilesetPaths[m_activeTilesetIndex];
                const size_t slash = fullPath.find_last_of("\\/");
                tilesetLabel = (slash == std::string::npos) ? fullPath : fullPath.substr(slash + 1);
            }

            ImGui::Text("Active Tileset");
            ImGui::SameLine();
            if (ImGui::BeginCombo("##ActiveTileset", tilesetLabel.c_str())) {
                for (size_t i = 0; i < m_tilesetPaths.size(); ++i) {
                    const bool isSelected = (i == m_activeTilesetIndex);
                    const std::string& fullPath = m_tilesetPaths[i];
                    const size_t slash = fullPath.find_last_of("\\/");
                    const std::string displayName = (slash == std::string::npos) ? fullPath : fullPath.substr(slash + 1);
                    if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                        m_activeTilesetIndex = static_cast<uint8_t>(i);
                        m_tileset = (i < m_tilesets.size()) ? m_tilesets[i] : nullptr;
                        m_selectedTileID = 0; // Reset tile selection on tileset switch.
                        if (m_activeTilesetCallback) {
                            m_activeTilesetCallback(m_activeTilesetIndex);
                        }
                    }
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
            // Use TextDisabled for a gray "placeholder" look, consistent with Property Editor
            ImGui::TextDisabled("Drag and drop an asset here");
        }
        else {
            // Toolbar: Eraser, Rotate
            if (m_collisionEditActive) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button(m_paintMode ? "Paint Mode: ON" : "Paint Mode: OFF")) {
                if (!m_collisionEditActive) {
                    m_paintMode = !m_paintMode;
                }
            }
            if (m_collisionEditActive) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Disabled while collision editing");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(m_isEraser ? "Eraser [ON]" : "Eraser"))
            {
                m_isEraser = !m_isEraser;
            }
            ImGui::SameLine();
            ImGui::Text("Rotation: %d deg", m_currentRotation * 90);
            ImGui::SameLine();
            if (ImGui::Button("Rotate (R)"))
            {
                m_currentRotation = (m_currentRotation + 1) % 4;
            }

            ImGui::Separator();

            // Palette Grid
            float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            const auto& tiles = m_tileset->GetTiles();
            
            ImGuiStyle& style = ImGui::GetStyle();
            float buttonSize = 64.0f;
            
            int i = 0;
            for (const auto& [id, def] : tiles)
            {
                ImGui::PushID(id);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Remove tile button background.
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0)); // Keep hover background transparent.
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0)); // Keep active background transparent.
                
                // Calculate UVs for ImageButton
                ImVec2 uv0(def.uv.u0, def.uv.v1); // ImGui uses top-left origin? 
                // OpenGL coords: v=0 is bottom. ImGui: v=0 is top.
                // If texture is loaded standard OpenGL way (0,0 bottom-left), then:
                // ImGui (0,0) is top-left.
                // If we draw full texture, uv0=(0,1), uv1=(1,0) flips it.
                // TileUV is u0,v0 (bottom-left?), u1,v1 (top-right?).
                // Let's assume TileUV is standard OpenGL (0,0 is bottom-left).
                // ImGui expects uv0=top-left, uv1=bottom-right.
                // So uv0 = (u0, v1), uv1 = (u1, v0).
                ImVec2 imUV0(def.uv.u0, def.uv.v1); 
                ImVec2 imUV1(def.uv.u1, def.uv.v0);

                // Highlight selected
                std::string strId = "Tile_" + std::to_string(id);
                if (ImGui::ImageButton(strId.c_str(),(ImTextureID)(uintptr_t)m_tileset->GetTextureId(), ImVec2(buttonSize, buttonSize), imUV0, imUV1))
                {
                    m_selectedTileID = id;
                    m_isEraser = false;
                    m_paintMode = true;
                }

                const bool isHovered = ImGui::IsItemHovered(); // Track hover state for custom border drawing.
                const bool isSelected = (m_selectedTileID == id && !m_isEraser); // Track selection state for border.
                if (isHovered || isSelected) {
                    // Draw a colored border instead of changing button background.
                    const ImU32 borderColor = isSelected
                        ? ImGui::GetColorU32(ImVec4(1.0f, 0.85f, 0.15f, 1.0f))
                        : ImGui::GetColorU32(ImVec4(0.65f, 0.75f, 0.95f, 0.85f));
                    const ImVec2 min = ImGui::GetItemRectMin();
                    const ImVec2 max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(min, max, borderColor, 0.0f, 0, 2.0f);
                }

                float lastButtonX = ImGui::GetItemRectMax().x;
                float nextButtonX = lastButtonX + style.ItemSpacing.x + buttonSize;
                if (i + 1 < tiles.size() && nextButtonX < windowVisibleX)
                    ImGui::SameLine();

                ImGui::PopStyleColor(3);
                ImGui::PopID();
                i++;
            }
            
            // Handle 'R' key for rotation
            if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_R))
            {
                 m_currentRotation = (m_currentRotation + 1) % 4;
            }
        }

		// Collision Edit Section
        if (m_tileMap) {
            ImGui::BeginChild("##CollisionEditSection", ImVec2(0.0f, 0.0f), true);

			// Check if we have the necessary data to enable collision editing
            const bool hasTileMap = (m_tileMap != nullptr);
            const bool hasTileset = (m_tileset != nullptr);

			// Activate collision edit mode on click if we have a valid tilemap and tileset, 
            // and we're not already in collision edit mode
            if (!m_collisionEditActive && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                SetCollisionEditActive(true);
            }

            const bool collisionToolsEnabled = m_collisionEditActive && hasTileMap && hasTileset;
            const bool sectionDisabled = !m_collisionEditActive;

			// If we don't have the necessary data to edit collisions, we disable the entire section and show a tooltip on hover
            // Just in case
            if (sectionDisabled) {
                ImGui::BeginDisabled();
            }

			// Section header and instructions
            if (m_boldFont) ImGui::PushFont(m_boldFont);
            ImGui::Text("Collision Edit");
            if (m_boldFont) ImGui::PopFont();
            ImGui::TextDisabled("LMB: paint collision");

			// Toggle collision edit mode with a button as well, for discoverability
            if (ImGui::Button("Exit")) {
                SetCollisionEditActive(false);
            }
            ImGui::SameLine();

			// Collision eraser toggle separate from the normal paint mode eraser, since we want to be able to erase specific corners 
            // of collision without affecting the tile painting mode
            const char* eraserLabel = m_collisionEraser ? "Collision Eraser [ON]" : "Collision Eraser";
            if (!collisionToolsEnabled) ImGui::BeginDisabled();
            if (ImGui::Button(eraserLabel)) {
                m_collisionEraser = !m_collisionEraser;
            }
            if (!collisionToolsEnabled) ImGui::EndDisabled();
            ImGui::Separator();

			// Brush selection for which corners to paint
            // 2x2 grid representing the 4 corners of the tile, with buttons to toggle each corner's collision bit
            ImGui::Text("Brush");

			// Lambda to draw each brush cell with appropriate active/inactive styling based on the current brush mask
            const float cellSize = ImGui::GetFrameHeight();
            auto drawBrushCell = [&](const char* id, uint8_t bit) {
				// Determine if this corner is active in the brush mask
                const bool active = (m_collisionBrushMask & bit) != 0;

				// Style active corners with the success color, inactive corners with the secondary color
                ImGui::PushStyleColor(ImGuiCol_Button, active ? EditorStyle::SuccessButton : EditorStyle::SecondaryButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? EditorStyle::SuccessButtonHover : EditorStyle::SecondaryButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? EditorStyle::SuccessButtonActive : EditorStyle::SecondaryButtonActive);
                
				// When a brush cell is clicked, we toggle the corresponding bit in the collision brush mask
                if (ImGui::Button(id, ImVec2(cellSize, cellSize))) {
                    m_collisionEraser = false;
                    if (active) {
						// Clear the bit to deactivate this corner in the brush
                        m_collisionBrushMask = static_cast<uint8_t>(m_collisionBrushMask & ~bit);
                    } else {
						// Set the bit to activate this corner in the brush
                        m_collisionBrushMask = static_cast<uint8_t>(m_collisionBrushMask | bit);
                    }
                }
                ImGui::PopStyleColor(3);
            };

			// Disable the brush controls if collision tools aren't enabled
            // to prevent interaction when we don't have a valid tilemap/tileset or we're not in collision edit mode
            if (!collisionToolsEnabled) ImGui::BeginDisabled();

			// We use a 2x2 grid of buttons to represent the 4 corners of the tile for collision painting
            ImGui::PushID("CollisionBrushGrid");
			drawBrushCell("##TL", kCollisionMaskTopLeft);       // Top-left corner
            ImGui::SameLine();
			drawBrushCell("##TR", kCollisionMaskTopRight);      // Top-right corner
			drawBrushCell("##BL", kCollisionMaskBottomLeft);    // Bottom-left corner
            ImGui::SameLine();  
			drawBrushCell("##BR", kCollisionMaskBottomRight);   // Bottom-right corner
            ImGui::PopID();
            ImGui::Separator();

			// Presets for common collision configurations to speed up editing
            // These set the collision brush mask to predefined values representing full, empty or single-corner collisions
            ImGui::Text("Presets");
            const struct { const char* label; const char* tooltip; uint8_t mask; } presets[] = {
                {"F", "Full", 0x0F},
                {"E", "Empty", 0x00},
                {"T", "Top", 0x0C},
                {"B", "Bottom", 0x03},
                {"L", "Left", 0x05},
                {"R", "Right", 0x0A}
            };

			// We iterate over the presets and create a button for each
            constexpr size_t presetCount = sizeof(presets) / sizeof(presets[0]);
            for (size_t i = 0; i < presetCount; i++) {
                if (i > 0) ImGui::SameLine();

                // Clicking a preset sets the collision brush mask to the preset's value
                if (ImGui::Button(presets[i].label)) {
                    m_collisionEraser = false;
                    m_collisionBrushMask = presets[i].mask;
                }
				// Show tooltips for the preset buttons to explain what they do
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(presets[i].tooltip);
                }
            }
            if (!collisionToolsEnabled) ImGui::EndDisabled();

			// If we don't have a valid tilemap or tileset, we disable the collision edit section 
            // and show a tooltip on hover to explain why
            if (sectionDisabled) {
                ImGui::EndDisabled();
            }

            ImGui::EndChild();
        }

    }
    ImGui::End();
}

/*------------------------------------------------------------------*/
/*!
\brief Draws a tile preview overlay while hovering inside the viewport.

Converts world coordinates into tilemap coordinates and renders a
semi-transparent preview using the renderer system. Supports rotation
hotkeys and eraser visualization.

\param worldPos Cursor position in world space
*/

void TilePalettePanel::OnViewportHover(const glm::vec2& worldPos)
{
    if (!m_tileMap || !m_tileset) return;

    // Handle 'R' key globally when hovering viewport
    if (ImGui::IsKeyPressed(ImGuiKey_R))
    {
        m_currentRotation = (m_currentRotation + 1) % 4;
    }

    const glm::vec2 localPos = worldPos - m_worldOrigin; // Convert world space to tilemap-local space.
    // Compute signed tile indices so negative coordinates are supported.
    const int32_t tx = m_tileMap->WorldToTileSigned(localPos.x); // Signed tile coordinate in map space.
    const int32_t ty = m_tileMap->WorldToTileSigned(localPos.y); // Signed tile coordinate in map space.

    // Calculate tile world-space bounds for rendering preview overlay.
    float tileSize = m_tileMap->TileSize();
    glm::vec2 tilePos(m_tileMap->TileToWorldSigned(tx), m_tileMap->TileToWorldSigned(ty)); // Tile origin in local space.
    tilePos += m_worldOrigin; // Shift tile origin into world space.
    glm::vec2 size(tileSize, tileSize);

    ECS::RendererSystem* rs = ECS::RendererSystem::GetInstance();
    if (rs) {
        glm::vec2 min(tilePos.x, tilePos.y);
        glm::vec2 max(tilePos.x + size.x, tilePos.y + size.y);
        if (m_isEraser) {
            glm::vec4 color(1.0f, 0.0f, 0.0f, 0.35f); // Red translucent preview for eraser.
            rs->SubmitFilledQuad(min, max, color);
            return;
        }

        TileUV uv;
        if (!m_tileset->GetTileUV(m_selectedTileID, uv)) {
            glm::vec4 color(1.0f, 1.0f, 1.0f, 0.25f); // Fallback to a light fill if UV is missing.
            rs->SubmitFilledQuad(min, max, color);
            return;
        }

        const glm::vec4 uvRect(uv.u0, uv.v0, uv.u1, uv.v1); // Pack UVs for submitQuad.
        const glm::vec2 center(tilePos.x + size.x * 0.5f, tilePos.y + size.y * 0.5f);
        const glm::vec4 tint(1.0f, 1.0f, 1.0f, 0.5f); // Translucent tile preview.
        const float rotation = static_cast<float>(m_currentRotation) * 1.57079632679f;
        rs->SubmitOverlayQuad(center, size, m_tileset->GetTextureId(), uvRect, tint, rotation);
    }
}

/*------------------------------------------------------------------*/
/*!
\brief Draws collision brush hover preview inside the viewport.

\param worldPos Cursor position in world space
*/
void TilePalettePanel::OnViewportCollisionHover(const glm::vec2& worldPos)
{
	// Only draw collision preview if we're in collision edit mode and have a valid tilemap and tileset
    if (!m_collisionEditActive || !m_tileMap || !m_tileset) return;

	// Convert world position to tilemap-local coordinates to determine which tile we're hovering over
    ECS::RendererSystem* rs = ECS::RendererSystem::GetInstance();
    if (!rs) return;

	// Calculates which tile the mouse is currently hovering over
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

	// Calculate the world-space position of the tile's bottom-left corner for rendering the preview
    const glm::vec2 tileWorld(m_worldOrigin.x + m_tileMap->TileToWorldSigned(tx), 
        m_worldOrigin.y + m_tileMap->TileToWorldSigned(ty));

	// We divide the tile into 4 quadrants for the 4 corners of collision
    // The brush mask determines which corners are active
    const float subSize = tileSize * 0.5f;

	// If the collision eraser is active, we draw a red translucent overlay over the entire tile to 
    // indicate that we're erasing collision from this tile
    if (m_collisionEraser) {
        const glm::vec4 eraseColor(0.9f, 0.2f, 0.2f, 0.25f);
        rs->SubmitFilledQuad(tileWorld, tileWorld + glm::vec2(tileSize, tileSize), eraseColor);
        return;
    }

	// The collision brush mask uses the lower 4 bits to represent which corners of the tile will have 
    // collision painted
    const uint8_t mask = static_cast<uint8_t>(m_collisionBrushMask & 0x0F);
    if (mask == 0) return;

	// For each corner of the tile, if the corresponding bit in the brush mask is set, we draw a green translucent 
    // quad over that quadrant to preview the collision that will be painted
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

/*------------------------------------------------------------------*/
/*!
\brief Applies tile paint or erase operation.

Handles coordinate conversion, map expansion, undo integration,
physics syncing, and persistence updates.

\param worldPos Cursor position
\param isRightClick True if erase input
\return True if tile changed
*/
bool TilePalettePanel::OnViewportClick(const glm::vec2& worldPos, bool isRightClick)
{
    if (!m_tileMap || !m_tileset) return false;

    const glm::vec2 localPos = worldPos - m_worldOrigin; // Convert world space to tilemap-local space.
    const int32_t tx = m_tileMap->WorldToTileSigned(localPos.x); // Signed tile coordinate in map space.
    const int32_t ty = m_tileMap->WorldToTileSigned(localPos.y); // Signed tile coordinate in map space.
    const int64_t key = PackCoord(tx, ty);

    // Check bounds? TileMap expands? TileMap has fixed size layers.
    // Assuming layer 0.
    if (m_tileMap->LayerCount() == 0) return false;
    const uint32_t kExpandMargin = 2; // Expand when painting within N tiles of the edge.
    const uint32_t kExpandStep = 16; // Grow by this many tiles per expansion.

    m_tileMap->ExpandLayerToFit(0, tx, ty, kExpandMargin, kExpandStep); // Grow the layer to fit signed coords.
    if (!m_tileMap->IsTileInBounds(tx, ty)) return false; // Bail out if still out of bounds.

    bool erasing = isRightClick || m_isEraser;
    if (m_hasLastPaint && key == m_lastPaintKey && erasing == m_lastPaintErase) {
        return false; // Skip redundant paint when dragging over the same tile.
    }
    
	// Perform the tile paint/erase operation.
    TileID oldTile = m_tileMap->GetTileSigned(0, tx, ty);

	// Determine new tile ID based on eraser state.
    TileID newTile = EMPTY_TILE;

	// If not erasing, pack the selected tile with rotation and tileset index.
    if (!erasing) {
        newTile = PackTile(m_selectedTileID, m_currentRotation, m_activeTilesetIndex);
    }

	// No-op if tile is unchanged.
    if (oldTile == newTile) {
        return false;
    }

	// Define the tile changed callback to sync physics and handle saves.
    auto onTileChanged = [this](int32_t x, int32_t y, TileID id) {
		// Sync physics entities for this tile change.
        bool isEraser = (id == EMPTY_TILE);
        this->SyncPhysics(x, y, id, isEraser);

        // Mark the scene dirty so saves are enabled and tracked.
        Messaging::MessageSystem::Notify(Messaging::SceneModified("Tilemap paint"));

		// Auto-save the tilemap if we have a valid path.
        if (!m_tileMapPath.empty()) {
            // Persist edits immediately so the tilemap asset stays in sync.
            m_tileMap->SaveMap(m_tileMapPath);
        }
    };

	// Use undo system if available.
    if (m_undoSystem) {
		// Create and execute a TilePaintCommand for undo/redo support.
        auto command = std::make_unique<Editor::TilePaintCommand>(
            m_tileMap, tx, ty, oldTile, newTile, onTileChanged
        );
		// Execute the command via the undo system.
        m_undoSystem->ExecuteCommand(std::move(command));
    } 
	// Directly set the tile if no undo system is present.
    else {
        m_tileMap->SetTileSigned(0, tx, ty, newTile);
        onTileChanged(tx, ty, newTile);
    }

    m_hasLastPaint = true;
    m_lastPaintKey = key;
    m_lastPaintErase = erasing;
    return true;
}

/*------------------------------------------------------------------*/
/*!
\brief Applies collision mask paint action.

\param worldPos Cursor position
\return True if collision mask changed
*/
bool TilePalettePanel::OnViewportCollisionClick(const glm::vec2& worldPos)
{
	// Collision editing is only active when the collision edit mode is enabled, 
    // and we have a valid tilemap and tileset to work with
    if (!m_collisionEditActive || !m_tileMap || !m_tileset) return false;
    if (m_tileMap->LayerCount() == 0) return false;

    // Convert world space to tilemap-local space
    const glm::vec2 localPos = worldPos - m_worldOrigin; 

	// Calculate signed tile coordinates to support editing in negative space,
    // then pack into a single key for tracking
    const int32_t tx = m_tileMap->WorldToTileSigned(localPos.x);
    const int32_t ty = m_tileMap->WorldToTileSigned(localPos.y);
    const int64_t key = PackCoord(tx, ty);

	// Only allow collision painting on tiles that exist
    if (m_tileMap->GetTileSigned(0, tx, ty) == EMPTY_TILE) {
        return false;
    }

	// Collision editing only modifies the collision mask, so we don't need to worry about tile changes here
    const uint32_t kExpandMargin = 2;  // Expand when painting within N tiles of the edge
    const uint32_t kExpandStep = 16;   // Grow by this many tiles per expansion

    // Ensure the tile coordinates are within bounds, expanding the layer if necessary to accommodate edits 
    // in negative space or near edges
    m_tileMap->ExpandLayerToFit(0, tx, ty, kExpandMargin, kExpandStep);
    if (!m_tileMap->IsTileInBounds(tx, ty)) return false;

	// Calculate the new collision mask based on the current brush mask and eraser state
    const uint8_t newMask = static_cast<uint8_t>(m_collisionEraser ? 0 : (m_collisionBrushMask & 0x0F));

    // Skip redundant paint when dragging over the same tile
    if (m_collisionHasLastPaint && key == m_collisionLastPaintKey && newMask == m_collisionLastPaintMask) {
        return false; 
    }

	// No-op if collision mask is unchanged to avoid unnecessary updates and saves
    const uint8_t oldMask = m_tileMap->GetCollisionMaskSigned(tx, ty);
    if (oldMask == newMask) {
        return false;
    }

	// Apply the new collision mask to the tilemap
    m_tileMap->SetCollisionMaskSigned(tx, ty, newMask);

	// Mark the scene dirty to enable saves and track changes
    Messaging::MessageSystem::Notify(Messaging::SceneModified("Tilemap collision"));
    if (!m_tileMapPath.empty()) {
        m_tileMap->SaveMap(m_tileMapPath);
    }

	// Update paint tracking state for collision editing to optimize drag painting and avoid redundant updates
    m_collisionHasLastPaint = true;
    m_collisionLastPaintKey = key;
    m_collisionLastPaintMask = newMask;
    return true;
}

/*------------------------------------------------------------------*/
/*!
\brief Synchronizes ECS physics colliders with tile state.

Removes stale entities and spawns new static collider entities
based on tile collision metadata.

\param x Tile X coordinate
\param y Tile Y coordinate
\param id Packed tile ID
\param isEraser True if tile removed
*/
void TilePalettePanel::SyncPhysics(int32_t x, int32_t y, TileID id, bool isEraser)
{
    if (!m_world) return;

    int64_t key = PackCoord(x, y);
    
    // Always remove existing entity at this location
    auto it = m_physicsEntities.find(key);
    if (it != m_physicsEntities.end())
    {
        if (m_world->IsAlive(it->second))
        {
            m_world->Destroy(it->second);
        }
        m_physicsEntities.erase(it);
    }

    if (isEraser) return;

    // If adding a tile, check collision
    TileID baseID = GetTileBaseID(id);
    uint8_t tilesetIndex = GetTileTilesetIndex(id);
    if (tilesetIndex >= m_tilesets.size() || !m_tilesets[tilesetIndex]) {
        return; // Missing tileset, so skip collider setup.
    }
    CollisionType type = m_tilesets[tilesetIndex]->GetCollisionType(baseID);
    
    if (type == CollisionType::NONE) return;

    // Spawn Entity
    // Position: TileToWorld gives bottom-left (or top-left?). 
    // BoxCollider2D expects center.
    float tileSize = m_tileMap->TileSize();
    float halfSize = tileSize * 0.5f;
    glm::vec2 pos(m_tileMap->TileToWorldSigned(x) + halfSize, m_tileMap->TileToWorldSigned(y) + halfSize);
    pos += m_worldOrigin; // Move collider position into world space using tilemap origin.
    
    // Create Entity
    ECS::Entity e = m_world->Create();
    
    // Add Transform
    ECS::Components::LocalTransform trans;
    trans.Position = { pos.x, pos.y, 0.0f }; // Z=0
    m_world->Add<ECS::Components::LocalTransform>(e, trans);
    
    // Add BoxCollider2D
    ECS::Components::BoxCollider2D collider;
    collider.HalfExtents = { halfSize, halfSize };
    
    // Handle Rotation
    uint8_t rotIdx = GetTileRotation(id);
    collider.Rotation = rotIdx * 1.57079632679f; // 90 deg steps in radians
    
    m_world->Add<ECS::Components::BoxCollider2D>(e, collider);

    // Add Rigidbody2D (Static)
    ECS::Components::Rigidbody2D rb;
    rb.Mass = 0.0f; // Static
    rb.Flags = 0; // Fixed rotation? Maybe.
    m_world->Add<ECS::Components::Rigidbody2D>(e, rb);

    // Track it
    m_physicsEntities[key] = e;
}
