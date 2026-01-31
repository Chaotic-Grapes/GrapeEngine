/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file SpriteImportPanel.cpp
 * @brief Panel for importing 2D sprites with automatic pixel-perfect BoxCollider2D.
 *
 * 
 * 
 * 
 *  Note: in progress but halfway was displaced by "Generate AABB" button in property editor
 */


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
void SpriteImportPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont)
{
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;

    std::filesystem::create_directories("assets/sprites");
}

void SpriteImportPanel::SetWorld(ECS::World* world)
{
    m_world = world;
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------
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

    // Success feedback
    /*char msg[512];
    sprintf_s(msg,
        "Sprite imported successfully!\n\n"
        "File: %s\n"
        "Size: %d × %d pixels\n"
        "Collider HalfExtents: ±%.1f, ±%.1f",
        filename.c_str(), pixelWidth, pixelHeight,
        pixelWidth * 0.5f, pixelHeight * 0.5f);

    MessageBoxA(g_MainWindowHandle, msg, "Sprite Importer", MB_ICONINFORMATION);*/

    return true;
}


// -------------------------------------------------------------------------
// Entity Creation (for this test case)
// -------------------------------------------------------------------------
//bool SpriteImportPanel::_createSpriteEntity(const std::string& imagePath, int pixelWidth, int pixelHeight)
//{
//    using namespace ECS::Components;
//
//    // Create Entity
//    ECS::Entity entity;// = HierarchyPanel::m_selectedEntityId;
//
//   /* m_world->Add<LocalTransform>(entity);
//    m_world->Add<SpriteRenderer2D>(entity);
//    m_world->Add<BoxCollider2D>(entity);
//    m_world->Add<Name>(entity);*/
//
//    //SpriteRenderer2D::TextureId;
//
//    // Transform - centered origin
//    auto& transform = m_world->Get<LocalTransform>(entity);
//    transform.Position = Vector3D{ 0.0f, 0.0f, 0.0f };
//    transform.Scale = Vector3D{ 1.0f, 1.0f, 1.0f };
//
//    // Sprite
//    auto& sprite = m_world->Get<SpriteRenderer2D>(entity);
//    sprite.TextureId = RM.Get<Texture>(imagePath.c_str())->ID();
//    sprite.Width = pixelWidth;
//    sprite.Height = pixelHeight;
//    sprite.Color = Color{ 1.0f, 1.0f, 1.0f, 1.0f };
//
//    // Auto BoxCollider2D - pixel-perfect, centered
//    auto& col = m_world->Get<BoxCollider2D>(entity);
//    col.HalfExtents = Vector2D{ pixelWidth * 0.5f, pixelHeight * 0.5f };
//    col.Offset = Vector2D{ 0.0f, 0.0f };
//    col.LayerMask = 0xFFFFFFFFu;
//    col.Flags = 0;  // Not a trigger
//
//    // Name the entity after the file (without extension)
//    auto& nameComp = m_world->Get<Name>(entity);
//    std::string filename = std::filesystem::path(imagePath).filename().stem().string();
//    strncpy_s(nameComp.Value, filename.c_str(), sizeof(nameComp.Value) - 1);
//
//    // Success feedback
//    /*char msg[512];
//    sprintf_s(msg,
//        "Sprite imported successfully!\n\n"
//        "File: %s\n"
//        "Size: %d × %d pixels\n"
//        "Collider HalfExtents: ±%.1f, ±%.1f",
//        filename.c_str(), pixelWidth, pixelHeight,
//        pixelWidth * 0.5f, pixelHeight * 0.5f);
//
//    MessageBoxA(g_MainWindowHandle, msg, "Sprite Importer", MB_ICONINFORMATION);*/
//
//    return true;
//}
