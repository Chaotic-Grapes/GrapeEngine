/* Start Header *****************************************************************/
/*!
\file   AnimationPreviewSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   29th January 2026
\brief
Implements the AnimationPreviewSystem which updates sprite sheet UVs for editor
preview without advancing animation time.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/AnimationPreviewSystem.h"
#include "ecs/Components.h"
#include "graphics/SpriteSheetUtils.h"
#include <algorithm>

namespace ECS {
    SystemMetadata AnimationPreviewSystem::GetMetadata() const {
        ComponentAccessBuilder builder("AnimationPreview");
        builder.ReadComponent<Components::SpriteSheetAnimation2D>();
        builder.ReadComponent<Components::AnimationState2D>();
        builder.ReadComponent<Components::Active>();
        builder.WriteComponent<Components::SpriteRenderer2D>();
        builder.SetExecutionOrder(190);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::EditOnly);
        return builder.Build();
    }

    void AnimationPreviewSystem::OnUpdate(World& world) {
        // Update sprite UVs for all entities with SpriteSheetAnimation2D and SpriteRenderer2D
        world.Each<Components::SpriteSheetAnimation2D, Components::SpriteRenderer2D>(
            [&](Entity entity, const Components::SpriteSheetAnimation2D& anim, Components::SpriteRenderer2D& sprite) {
                // Skip if entity is not active, needless to say
                if (const auto* active = world.TryGet<Components::Active>(entity)) {
                    if (!active->Enabled)
                        return;
                }

                // Validate animation parameters
                // If invalid, skip updating UVs
                if (anim.FrameWidth <= 0 || anim.FrameHeight <= 0 ||
                    anim.SheetWidth <= 0 || anim.SheetHeight <= 0)
                    return;

                // Compute layout and frame window
                const auto layout = SpriteSheetUtils::ComputeLayout(
                    anim.FrameWidth, anim.FrameHeight, anim.SheetWidth, anim.SheetHeight);

                // Validate layout
                // Columns and rows must be AT LEAST 1!!
                if (layout.TotalCols <= 0 || layout.TotalRows <= 0)
                    return;

                // Compute frame window based on mode
                SpriteSheetUtils::Window window{};
                if (anim.UseRow) { // Row-based window
                    window = SpriteSheetUtils::ComputeRowWindow(
                        anim.RowIndex, anim.RowStartColumn, anim.RowFrameCount,
                        layout.TotalCols, layout.TotalRows);
                } else { // Standard window (columns)
                    window = SpriteSheetUtils::ComputeWindow(
                        anim.StartFrame, anim.FrameCount,
                        layout.TotalCols, layout.TotalRows);
                }

                // No valid frames to display
                if (window.Count <= 0)
                    return;

                // Determine current frame to display
                int localFrame = 0;
                if (world.Has<Components::AnimationState2D>(entity)) {
                    localFrame = world.Get<Components::AnimationState2D>(entity).CurrentFrame;
                }
                // Clamp local frame within window
                localFrame = std::clamp(localFrame, 0, window.Count - 1);

                // Compute absolute frame index and UVs
                const int absoluteFrame = window.Start + localFrame;
                const glm::vec4 uv = SpriteSheetUtils::ComputeUV(
                    absoluteFrame, anim.FrameWidth, anim.FrameHeight, anim.SheetWidth, anim.SheetHeight);

                // Update sprite renderer UVs and size
                // Only if texture IDs are valid
                if (anim.TextureId != 0)
                    sprite.TextureId = anim.TextureId;
                if (anim.NormalTextureId != 0)
                    sprite.NormalTextureId = anim.NormalTextureId;
                
                // Finally, update sprite size and UVs
                sprite.Width = anim.FrameWidth;
                sprite.Height = anim.FrameHeight;
                sprite.Tiling = Vector2D{ uv.z - uv.x, uv.w - uv.y };
                sprite.Offset = Vector2D{ uv.x, uv.y };
            }
        );
    }
}
