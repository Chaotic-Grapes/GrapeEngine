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
    /**
     * @brief Store fonts for use during rendering.
     * @param mainFont Primary UI font.
     * @param boldFont Bold UI font.
     * @param symbolsFont Icon/symbol font.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);
    
    /**
     * @brief Set the ECS world used when creating sprite entities.
     * @param world Pointer to the active ECS world.
     */
    void SetWorld(ECS::World* world);

    /** @brief Render the sprite import panel UI. */
    void Render();

    /**
     * @brief Check whether a valid ECS world has been assigned.
     * @return True if the world pointer is non-null.
     */
    bool HasValidWorld() const { return m_world != nullptr; }

private:
    /**
     * @brief Create a new entity with Sprite and BoxCollider2D from the given image.
     * @param imagePath Path to the source image asset.
     * @param pixelWidth Image width in pixels (used to size the collider).
     * @param pixelHeight Image height in pixels (used to size the collider).
     * @return True if the entity was created successfully.
     */
    bool _createSpriteEntity(const std::string& imagePath, int pixelWidth, int pixelHeight);

    ECS::World* m_world = nullptr;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    char m_lastFolder[512] = "assets/sprites";
};

#endif
