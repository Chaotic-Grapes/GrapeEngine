


/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file AABBGenerator.h
 * @brief Panel for importing 2D sprites with automatic pixel-perfect BoxCollider2D.
 *
 */

#ifndef SPRITE_IMPORT_PANEL_H
#define SPRITE_IMPORT_PANEL_H

#include "ecs/World.h"
#include <imgui.h>

class SpriteImportPanel
{
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);
    void SetWorld(ECS::World* world);
    void Render();

    bool HasValidWorld() const { return m_world != nullptr; }

private:
    
    bool _createSpriteEntity(const std::string& imagePath, int pixelWidth, int pixelHeight);

    ECS::World* m_world = nullptr;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    char m_lastFolder[512] = "assets/sprites";
};
#endif