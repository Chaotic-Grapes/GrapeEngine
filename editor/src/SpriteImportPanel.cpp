/* Start Header *****************************************************************/
/*!
\file   SpriteImportPanel.cpp
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\brief
Panel for importing 2D sprites with automatic pixel-perfect BoxCollider2D generation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "SpriteImportPanel.h"
#include "ecs/World.h"
#include "ecs/Components.h"                
#include "ecs/StringTable.h"
#include "EditorECSUtils.h"
#include "services/ResourceManager.h"           
#include "core/Logger.h"
#include "graphics/Texture.hpp" 
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <stb_image.h>

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

/**
 * @brief Store font references and create the sprites output directory if it does not exist.
 * @param mainFont Primary UI font.
 * @param boldFont Bold UI font.
 * @param symbolsFont Icon/symbol font.
 */
void SpriteImportPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont)
{
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;

    std::filesystem::create_directories("assets/sprites");
}

/**
 * @brief Set the ECS world used when creating sprite entities from imported assets.
 * @param world Pointer to the ECS world (may be nullptr).
 */
void SpriteImportPanel::SetWorld(ECS::World* world)
{
    m_world = world;
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

/** @brief Render the sprite importer panel UI with file path input and import controls. */
void SpriteImportPanel::Render()
{
    ImGui::Begin("Sprite Importer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::PushFont(m_boldFont);
    ImGui::Text("Sprite Importer");
    ImGui::PopFont();

    ImGui::Separator();

    ImGui::PushFont(m_mainFont);
    ImGui::TextWrapped("Import a sprite and automatically generate a pixel-perfect centered BoxCollider2D.");

    ImGui::TextDisabled("Last folder:");
    ImGui::SameLine();
    ImGui::InputText("##folder", m_lastFolder, sizeof(m_lastFolder));

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (ImGui::Button("IMPORT SPRITE", ImVec2(-1.0f, 50.0f)))
    {
        
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "Pivot: Center");
    ImGui::TextDisabled("AABB = image width/height ÷ 2");
    ImGui::PopFont();

    ImGui::End();
}

// -------------------------------------------------------------------------
// Entity Creation (uses your real components)
// -------------------------------------------------------------------------

/**
 * @brief Create a new entity with a sprite renderer and a pixel-perfect BoxCollider2D from the given image.
 * @param imagePath Path to the source image file.
 * @param pixelWidth Width of the image in pixels.
 * @param pixelHeight Height of the image in pixels.
 * @return True if the entity was created successfully, false otherwise.
 */
bool SpriteImportPanel::_createSpriteEntity(const std::string& imagePath, int pixelWidth, int pixelHeight)
{
    // Create Entity
    ECS::Entity entity = m_world->Create();

    Editor::ECSUtils::SetComponent(m_world, entity, "LocalTransform", ECS::Components::LocalTransform{});
    Editor::ECSUtils::SetComponent(m_world, entity, "WorldTransform", ECS::Components::WorldTransform{});
    Editor::ECSUtils::SetComponent(m_world, entity, "Layer", ECS::Components::Layer{ 0 });
    Editor::ECSUtils::AddComponent(m_world, entity, "SpriteRenderer2D", ECS::Components::SpriteRenderer2D{});
    Editor::ECSUtils::AddComponent(m_world, entity, "BoxCollider2D", ECS::Components::BoxCollider2D{});

    // Transform - centered origin
    if (auto* transform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(m_world, entity, "LocalTransform")) {
        transform->Position = Vector3D{ 0.0f, 0.0f, 0.0f };
        transform->Scale = Vector3D{ 1.0f, 1.0f, 1.0f };
    }

    // Sprite
    if (auto* sprite = Editor::ECSUtils::GetComponentPtr<ECS::Components::SpriteRenderer2D>(m_world, entity, "SpriteRenderer2D")) {
        std::shared_ptr<Texture> tex = RM.Get<Texture>(imagePath.c_str());
        sprite->TextureId = RM.Get<Texture>(imagePath.c_str())->ID();
        sprite->Width = tex->Width();
        sprite->Height = tex->Height();
        sprite->Color = Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    }

    // Auto BoxCollider2D - pixel-perfect, centered
    if (auto* col = Editor::ECSUtils::GetComponentPtr<ECS::Components::BoxCollider2D>(m_world, entity, "BoxCollider2D")) {
        col->HalfExtents = Vector2D{ pixelWidth * 0.5f, pixelHeight * 0.5f };
        col->Offset = Vector2D{ 0.0f, 0.0f };
        col->LayerMask = 0xFFFFFFFFu;
        col->Flags = 0;  // Not a trigger
    }

    // Name the entity after the file (without extension)
    std::string filename = std::filesystem::path(imagePath).filename().stem().string();
    Editor::ECSUtils::SetEntityName(*m_world, entity, filename);

    return true;
}
