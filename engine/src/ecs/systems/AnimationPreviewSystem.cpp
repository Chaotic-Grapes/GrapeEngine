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

namespace {
    constexpr int kMaxAnimSegments = ECS::Components::SpriteSheetAnimation2D::MaxSegments;

    int BuildSegmentSpans(const ECS::Components::SpriteSheetAnimation2D& anim,
        int totalCols, int totalRows,
        int(&starts)[kMaxAnimSegments], int(&counts)[kMaxAnimSegments]) {
        if (totalCols <= 0 || totalRows <= 0)
            return 0;

        const int segCount = std::clamp(static_cast<int>(anim.SegmentCount), 0, kMaxAnimSegments);
        int totalCount = 0;
        for (int i = 0; i < segCount; ++i) {
            const int row = std::clamp(anim.SegmentRows[i], 0, totalRows - 1);
            const int startCol = std::clamp(anim.SegmentOffsets[i], 0, totalCols - 1);
            const int available = totalCols - startCol;

            int count = anim.SegmentLengths[i];
            if (count <= 0 || count > available) {
                count = available;
            }
            if (count <= 0) {
                starts[i] = 0;
                counts[i] = 0;
                continue;
            }

            starts[i] = row * totalCols + startCol;
            counts[i] = count;
            totalCount += count;
        }
        return totalCount;
    }

    int ResolveSegmentAbsoluteFrame(const int(&starts)[kMaxAnimSegments], const int(&counts)[kMaxAnimSegments],
        int segmentCount, int localFrame) {
        int cursor = 0;
        for (int i = 0; i < segmentCount; ++i) {
            const int count = counts[i];
            if (count <= 0) continue;
            if (localFrame < cursor + count) {
                return starts[i] + (localFrame - cursor);
            }
            cursor += count;
        }
        return -1;
    }
}

namespace ECS {
    SystemMetadata AnimationPreviewSystem::GetMetadata() const {
        ComponentAccessBuilder builder("AnimationPreview");
        builder.ReadComponent<Components::SpriteSheetAnimation2D>();
        builder.ReadComponent<Components::AnimationState2D>();
        builder.ReadComponent<Components::Active>();
        builder.ReadComponent<Components::Parent>();
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
                // Skip if entity or its parents are disabled.
                if (!world.IsActiveInHierarchy(entity)) {
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

                const bool useSegments = anim.UseSegments && anim.SegmentCount > 0;
                int clipCount = 0;
                int absoluteFrame = 0;

                // Compute frame selection based on mode.
                SpriteSheetUtils::Window window{};
                int segmentStarts[kMaxAnimSegments] = { 0 };
                int segmentCounts[kMaxAnimSegments] = { 0 };
                if (useSegments) {
                    clipCount = BuildSegmentSpans(anim, layout.TotalCols, layout.TotalRows, segmentStarts, segmentCounts);
                    if (clipCount <= 0)
                        return;
                }
                else if (anim.UseRow) { // Row-based window
                    window = SpriteSheetUtils::ComputeRowWindow(
                        anim.Row, anim.FrameOffset, anim.FrameLength,
                        layout.TotalCols, layout.TotalRows);
                    if (window.Count <= 0)
                        return;
                    clipCount = window.Count;
                } else { // Standard window (columns)
                    window = SpriteSheetUtils::ComputeWindow(
                        anim.StartFrame, anim.FrameCount,
                        layout.TotalCols, layout.TotalRows);
                    if (window.Count <= 0)
                        return;
                    clipCount = window.Count;
                }

                // Determine current frame to display
                int localFrame = 0;
                if (world.Has<Components::AnimationState2D>(entity)) {
                    localFrame = world.Get<Components::AnimationState2D>(entity).CurrentFrame;
                }
                // Clamp local frame within window
                localFrame = std::clamp(localFrame, 0, clipCount - 1);

                // Compute absolute frame index and UVs
                if (useSegments) {
                    const int segCount = std::clamp(static_cast<int>(anim.SegmentCount), 0, kMaxAnimSegments);
                    absoluteFrame = ResolveSegmentAbsoluteFrame(segmentStarts, segmentCounts, segCount, localFrame);
                    if (absoluteFrame < 0)
                        return;
                }
                else {
                    absoluteFrame = window.Start + localFrame;
                }
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
                sprite.TextureFilter = anim.TextureFilter;
            }
        );
    }
}
