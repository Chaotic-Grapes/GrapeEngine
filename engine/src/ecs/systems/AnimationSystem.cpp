/* Start Header *****************************************************************/
/*!
\file   AnimationSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   1st November 2025
\brief
Implements the AnimationSystem which manages sprite sheet animations in the ECS.

The system:
- Updates animation state based on frame timing
- Advances to the next frame when enough time has accumulated
- Handles looping and non-looping animations
- Updates SpriteRenderer2D component with correct UV coordinates
- Respects the Active component (skips disabled entities)

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/AnimationSystem.h"
#include "ecs/Components.h"
#include "graphics/SpriteSheetUtils.h"
#include <algorithm>
#include <vector>
#include "services/TimeSystem.h"

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
    SystemMetadata AnimationSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Animation");
        // Define which components this system reads from
        builder.ReadComponent<Components::SpriteSheetAnimation2D>();
        builder.ReadComponent<Components::AnimationState2D>();
        builder.ReadComponent<Components::Active>();
        builder.ReadComponent<Components::Parent>();
        // Define which components this system modifies
        builder.WriteComponent<Components::SpriteSheetAnimation2D>();
        builder.WriteComponent<Components::AnimationState2D>();
        builder.WriteComponent<Components::SpriteRenderer2D>();
        // Execution parameters
        builder.SetExecutionOrder(200);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    // Logic here is the similar to AnimationPreviewSystem
    void AnimationSystem::OnUpdate(World& world) {
        // Get the delta time since the last frame for animation timing
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        // Iterate through all entities that have animation and animation state components
        world.Each<Components::SpriteSheetAnimation2D, Components::AnimationState2D>(
            [&](Entity entity, Components::SpriteSheetAnimation2D& anim, Components::AnimationState2D& state) {
                // Skip processing if the entity or its parents are disabled.
                if (!world.IsActiveInHierarchy(entity)) {
                    return;
                }

                // Validate animation dimensions; skip if any dimension is invalid
                if (anim.FrameWidth <= 0 || anim.FrameHeight <= 0 ||
                    anim.SheetWidth <= 0 || anim.SheetHeight <= 0)
                    return;

                // Calculate the sprite sheet layout (columns and rows) based on frame dimensions
                const auto layout = SpriteSheetUtils::ComputeLayout(
                    anim.FrameWidth, anim.FrameHeight, anim.SheetWidth, anim.SheetHeight);
                if (layout.TotalCols <= 0 || layout.TotalRows <= 0)
                    return;

                const bool useSegments = anim.UseSegments && anim.SegmentCount > 0;
                int clipCount = 0;
                int absoluteFrame = 0;

                // Determine which frames to play based on animation mode (segment list, row-based, or frame-range)
                SpriteSheetUtils::Window window{};
                int segmentStarts[kMaxAnimSegments] = { 0 };
                int segmentCounts[kMaxAnimSegments] = { 0 };
                if (useSegments) {
                    clipCount = BuildSegmentSpans(anim, layout.TotalCols, layout.TotalRows, segmentStarts, segmentCounts);
                    if (clipCount <= 0)
                        return;
                }
                else if (anim.UseRow) {
                    window = SpriteSheetUtils::ComputeRowWindow(
                        anim.Row, anim.FrameOffset, anim.FrameLength,
                        layout.TotalCols, layout.TotalRows);
                    if (window.Count <= 0)
                        return;
                    clipCount = window.Count;
                }
                else {
                    window = SpriteSheetUtils::ComputeWindow(
                        anim.StartFrame, anim.FrameCount,
                        layout.TotalCols, layout.TotalRows);
                    if (window.Count <= 0)
                        return;
                    clipCount = window.Count;
                }

                // Reset frame index if it's out of bounds
                if (state.CurrentFrame < 0 || state.CurrentFrame >= clipCount)
                    state.CurrentFrame = 0;

                // Reset finished flag if animation is set to loop
                if (anim.Loop)
                    state.Finished = false;

                // Determine if animation should advance: must be playing, have valid FPS, and either loop or not yet finished
                const bool canAdvance = anim.Playing && anim.FramesPerSecond > 0.0f &&
                    (anim.Loop || !state.Finished);

                if (canAdvance) {
                    // Add elapsed time to accumulator for frame timing
                    state.TimeAccumulator += dt;

                    // Calculate how much time each frame should be displayed
                    const float frameTime = 1.0f / anim.FramesPerSecond;

                    // Advance animation frame when accumulated time exceeds frame duration
                    while (state.TimeAccumulator >= frameTime) {
                        state.TimeAccumulator -= frameTime;
                        state.CurrentFrame++;

                        // Wrap frame index or stop animation when reaching the end
                        if (state.CurrentFrame >= clipCount) {
                            if (anim.Loop) {
                                state.CurrentFrame = 0;
                                state.Finished = false;
                            }
                            else {
                                state.CurrentFrame = clipCount - 1;
                                state.Finished = true;
                                state.TimeAccumulator = 0.0f;
                                break;
                            }
                        }
                    }
                }

                // Calculate the absolute frame index in the sprite sheet and compute UV coordinates for rendering
                if (useSegments) {
                    const int segCount = std::clamp(static_cast<int>(anim.SegmentCount), 0, kMaxAnimSegments);
                    absoluteFrame = ResolveSegmentAbsoluteFrame(segmentStarts, segmentCounts, segCount, state.CurrentFrame);
                    if (absoluteFrame < 0)
                        return;
                }
                else {
                    absoluteFrame = window.Start + state.CurrentFrame;
                }
                const glm::vec4 uv = SpriteSheetUtils::ComputeUV(
                    absoluteFrame, anim.FrameWidth, anim.FrameHeight, anim.SheetWidth, anim.SheetHeight);

                // Update the sprite renderer with current frame data; only do this if the sprite component exists
                // Animation state is advanced regardless to maintain consistency even without a renderer
                if (auto* sprite = world.TryGet<Components::SpriteRenderer2D>(entity)) {
                    // Update SpriteRenderer2D
                    sprite->TextureId = anim.TextureId;
                    sprite->NormalTextureId = anim.NormalTextureId;
                    sprite->Width = anim.FrameWidth;
                    sprite->Height = anim.FrameHeight;
                    sprite->Tiling = Vector2D{ uv.z - uv.x, uv.w - uv.y };
                    sprite->Offset = Vector2D{ uv.x, uv.y };
                    sprite->TextureFilter = anim.TextureFilter;
                }
            }
        );
    }

}
