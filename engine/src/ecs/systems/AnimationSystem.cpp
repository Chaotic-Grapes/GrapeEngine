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

namespace ECS {
    SystemMetadata AnimationSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Animation");
        // Define which components this system reads from
        builder.ReadComponent<Components::SpriteSheetAnimation2D>();
        builder.ReadComponent<Components::AnimationState2D>();
        builder.ReadComponent<Components::Active>();
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
                // Skip processing if the entity is disabled
                if (const auto* active = world.TryGet<Components::Active>(entity)) {
                    if (!active->Enabled)
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

                // Determine which frames to play based on animation mode (row-based or frame-range based)
                SpriteSheetUtils::Window window{};
                if (anim.UseRow) {
                    window = SpriteSheetUtils::ComputeRowWindow(
                        anim.Row, anim.FrameOffset, anim.FrameLength,
                        layout.TotalCols, layout.TotalRows);
                } else {
                    window = SpriteSheetUtils::ComputeWindow(
                        anim.StartFrame, anim.FrameCount,
                        layout.TotalCols, layout.TotalRows);
                }

                if (window.Count <= 0)
                    return;

                // Reset frame index if it's out of bounds
                if (state.CurrentFrame < 0 || state.CurrentFrame >= window.Count)
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
                        if (state.CurrentFrame >= window.Count) {
                            if (anim.Loop) {
                                state.CurrentFrame = 0;
                                state.Finished = false;
                            }
                            else {
                                state.CurrentFrame = window.Count - 1;
                                state.Finished = true;
                                state.TimeAccumulator = 0.0f;
                                break;
                            }
                        }
                    }
                }

                // Calculate the absolute frame index in the sprite sheet and compute UV coordinates for rendering
                const int absoluteFrame = window.Start + state.CurrentFrame;
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
