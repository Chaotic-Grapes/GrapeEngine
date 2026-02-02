#include "TilePalettePanel.h"

#include <imgui.h>
#include <algorithm>
#include <iostream>

#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"
#include "../include/core/World/TileTypes.hpp"

// Assuming ImGui::ImageButton takes ImTextureID (void*)
// Tileset::GetTextureId() returns uint32_t (OpenGL ID).
// We cast it to ImTextureID.

void TilePalettePanel::Initialize(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset, ECS::World* world)
{
    m_tileMap = tileMap;
    m_tileset = tileset;
    m_world = world;
}

void TilePalettePanel::Render()
{
    if (!m_active) return;

    if (ImGui::Begin("Tile Palette", &m_active))
    {
        if (!m_tileset)
        {
            ImGui::Text("No Tileset Loaded");
            ImGui::End();
            return;
        }

        // Toolbar: Eraser, Rotate
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
            if (m_selectedTileID == id && !m_isEraser)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 0.0f, 0.5f));
            }

            std::string strId = "Tile_" + std::to_string(id);
            if (ImGui::ImageButton(strId.c_str(),(ImTextureID)(uintptr_t)m_tileset->GetTextureId(), ImVec2(buttonSize, buttonSize), imUV0, imUV1))
            {
                m_selectedTileID = id;
                m_isEraser = false;
            }

            if (m_selectedTileID == id && !m_isEraser)
            {
                ImGui::PopStyleColor();
            }

            float lastButtonX = ImGui::GetItemRectMax().x;
            float nextButtonX = lastButtonX + style.ItemSpacing.x + buttonSize;
            if (i + 1 < tiles.size() && nextButtonX < windowVisibleX)
                ImGui::SameLine();

            ImGui::PopID();
            i++;
        }
        
        // Handle 'R' key for rotation
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_R))
        {
             m_currentRotation = (m_currentRotation + 1) % 4;
        }
    }
    ImGui::End();
}

void TilePalettePanel::OnViewportHover(const glm::vec2& worldPos)
{
    if (!m_tileMap || !m_tileset) return;

    // Handle 'R' key globally when hovering viewport
    if (ImGui::IsKeyPressed(ImGuiKey_R))
    {
        m_currentRotation = (m_currentRotation + 1) % 4;
    }

    uint32_t tx = m_tileMap->WorldToTile(worldPos.x);
    uint32_t ty = m_tileMap->WorldToTile(worldPos.y);

    float tileSize = m_tileMap->TileSize();
    glm::vec2 tilePos(m_tileMap->TileToWorld(tx), m_tileMap->TileToWorld(ty));
    glm::vec2 size(tileSize, tileSize);

    ECS::RendererSystem* rs = ECS::RendererSystem::GetInstance();
    if (rs) {
        auto* r = rs->GetRenderer();
        if (r) {
            glm::vec2 min(tilePos.x, tilePos.y);
            glm::vec2 max(tilePos.x + size.x, tilePos.y + size.y);
            glm::vec4 color = m_isEraser ? glm::vec4(1.0f, 0.0f, 0.0f, 0.4f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.4f);
            DebugDraw2D::RectFill(*r, min, max, color, 0);
        }
    }
}

bool TilePalettePanel::OnViewportClick(const glm::vec2& worldPos, bool isRightClick)
{
    if (!m_tileMap || !m_tileset) return false;

    uint32_t tx = m_tileMap->WorldToTile(worldPos.x);
    uint32_t ty = m_tileMap->WorldToTile(worldPos.y);
    
    // Check bounds? TileMap expands? TileMap has fixed size layers.
    // Assuming layer 0.
    if (m_tileMap->LayerCount() == 0) return false;
    const auto& layer = m_tileMap->GetLayer(0);
    if (tx >= layer.Width() || ty >= layer.Height()) return false;

    bool erasing = isRightClick || m_isEraser;
    
    if (erasing)
    {
        m_tileMap->SetTile(0, tx, ty, EMPTY_TILE);
        SyncPhysics(tx, ty, EMPTY_TILE, true);
    }
    else
    {
        TileID packed = PackTile(m_selectedTileID, m_currentRotation);
        m_tileMap->SetTile(0, tx, ty, packed);
        SyncPhysics(tx, ty, packed, false);
    }

    return true;
}

void TilePalettePanel::SyncPhysics(uint32_t x, uint32_t y, TileID id, bool isEraser)
{
    if (!m_world) return;

    uint32_t key = PackCoord(x, y);
    
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
    CollisionType type = m_tileset->GetCollisionType(baseID);
    
    if (type == CollisionType::NONE) return;

    // Spawn Entity
    // Position: TileToWorld gives bottom-left (or top-left?). 
    // BoxCollider2D expects center.
    float tileSize = m_tileMap->TileSize();
    float halfSize = tileSize * 0.5f;
    glm::vec2 pos(m_tileMap->TileToWorld(x) + halfSize, m_tileMap->TileToWorld(y) + halfSize);
    
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
    
    // Adjust Collider based on Diagonal?
    // User said: "If it is SOLID or DIAGONAL, spawn a static ECS Entity... using Rigidbody2D and BoxCollider2D."
    // For diagonal, a BoxCollider is an approximation unless we have PolygonCollider.
    // User instructions: "Use the ECS::Components::BoxCollider2D component."
    // So we use BoxCollider even for slopes (maybe rotated?).
    // "When syncing a rotated tile, convert the rotation bits... and set the BoxCollider2D::Rotation field."
    // So we just use BoxCollider with rotation.
    
    m_world->Add<ECS::Components::BoxCollider2D>(e, collider);

    // Add Rigidbody2D (Static)
    ECS::Components::Rigidbody2D rb;
    rb.Mass = 0.0f; // Static
    rb.Flags = 0; // Fixed rotation? Maybe.
    m_world->Add<ECS::Components::Rigidbody2D>(e, rb);

    // Track it
    m_physicsEntities[key] = e;
}
