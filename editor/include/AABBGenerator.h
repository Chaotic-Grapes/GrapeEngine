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
    /**
     * @brief Initialize panel resources and font references.
     * @param mainFont Main UI font.
     * @param boldFont Bold UI font.
     * @param symbolsFont Icon/symbol font.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    /**
     * @brief Assign the active ECS world used by this panel.
     * @param world Target world pointer.
     */
    void SetWorld(ECS::World* world);

    /**
     * @brief Render the sprite import panel UI.
     */
    void Render();

    /**
     * @brief Check whether the panel has a valid world reference.
     * @return True when world is non-null.
     */
    bool HasValidWorld() const { return m_world != nullptr; }

private:
    /**
     * @brief Create a sprite entity from imported texture metadata.
     * @param imagePath Path to the source image.
     * @param pixelWidth Imported sprite width in pixels.
     * @param pixelHeight Imported sprite height in pixels.
     * @return True when entity creation succeeds.
     */
    bool _createSpriteEntity(const std::string& imagePath, int pixelWidth, int pixelHeight);

    ECS::World* m_world = nullptr;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    char m_lastFolder[512] = "assets/sprites";
};
#endif