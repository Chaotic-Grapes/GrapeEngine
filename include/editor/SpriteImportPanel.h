/* Start Header *****************************************************************/
/*!
\file   SpriteImportPanel.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   27th November 2025
\brief
Header for SpriteImportPanel class - panel for importing 2D sprites with
automatic pixel-perfect BoxCollider2D.
*/
/* End Header *******************************************************************/

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
