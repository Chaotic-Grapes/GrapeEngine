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
#include <algorithm>
#include <vector>
#include "services/TimeSystem.h"

namespace ECS {
    SystemMetadata AnimationSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Animation");
        // Read accesses
        builder.ReadComponent<Components::SpriteSheetAnimation2D>();
        builder.ReadComponent<Components::AnimationState2D>();
        builder.ReadComponent<Components::Active>();
        // Write accesses
        builder.WriteComponent<Components::SpriteSheetAnimation2D>();
        builder.WriteComponent<Components::AnimationState2D>();
        builder.WriteComponent<Components::SpriteRenderer2D>();
        // Execution parameters
        builder.SetExecutionOrder(200);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    void AnimationSystem::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        // Process all entities with animation components
        world.Each<Components::SpriteSheetAnimation2D, Components::AnimationState2D>(
            [&](Entity entity, Components::SpriteSheetAnimation2D& anim, Components::AnimationState2D& state) {
                // Skip disabled entities
                if (const auto* active = world.TryGet<Components::Active>(entity)) {
                    if (!active->Enabled)
                        return;
                }

                // Skip if animation is not playing
                if (!anim.Playing)
                    return;

                // Skip if already finished (for non-looping animations)
                if (state.Finished && !anim.Loop)
                    return;

                // Invalid configuration check
                if (anim.FrameCount <= 0 || anim.FramesPerSecond <= 0.0f)
                    return;

                // Accumulate time
                state.TimeAccumulator += dt;

                // Calculate time per frame
                const float frameTime = 1.0f / anim.FramesPerSecond;

                // Advance frames if enough time has passed
                while (state.TimeAccumulator >= frameTime) {
                    state.TimeAccumulator -= frameTime;
                    state.CurrentFrame++;

                    // Handle frame wrapping
                    if (state.CurrentFrame >= anim.FrameCount) {
                        if (anim.Loop) { // Since it's looping, reset variables
                            state.CurrentFrame = 0; // Reset to first frame
                            state.Finished = false; // Reset finished flag
                        }
                        else {
                            // Non-looping animation finished
                            state.CurrentFrame = anim.FrameCount - 1; // Clamp to last frame since while loop advances it
                            state.Finished = true;                    // Mark as finished
                            state.TimeAccumulator = 0.0f;             // Clear accumulator
                            break;
                        }
                    }
                }

                // Calculate the absolute frame index in the sprite sheet
                const int absoluteFrame = anim.StartFrame + state.CurrentFrame;

                // Only compute sprite-sheet layout and UVs if a SpriteRenderer2D exists for this entity (reduce unnecessary work)
                // Also, advance animation state above even when no sprite is present
                if (auto* sprite = world.TryGet<Components::SpriteRenderer2D>(entity)) {
                    // Calculate sprite sheet layout
                    const int cols = (anim.SheetWidth > 0 && anim.FrameWidth > 0) // Check. Also avoid division by zero
                        ? std::max(1, anim.SheetWidth / anim.FrameWidth) : 1;     // Clamp to at least 1

                    const int rows = (anim.SheetHeight > 0 && anim.FrameHeight > 0)
                        ? std::max(1, anim.SheetHeight / anim.FrameHeight) : 1;

                    // Extra safety check
                    if (cols <= 0 || rows <= 0)
                        return;

                    // Calculate row and column for this frame
                    const int col = absoluteFrame % cols; // Column index
                    const int row = absoluteFrame / cols; // Row index

                    // Calculate UV coordinates (normalized 0-1)
                    // (col     * frameWidth, row     * frameHeight) = top-left corner
                    // (col + 1 * frameWidth, row + 1 * frameHeight) = bottom-right corner
                    // (col     * frameWidth, row + 1 * frameHeight) = bottom-left corner
                    // (col + 1 * frameWidth, row     * frameHeight) = top-right corner
                    const float u0 =       static_cast<float>(col * anim.FrameWidth)  / static_cast<float>(anim.SheetWidth);
                    const float v0 =       static_cast<float>(row * anim.FrameHeight) / static_cast<float>(anim.SheetHeight);
                    // const float u1 = u0 + (static_cast<float>(anim.FrameWidth)        / static_cast<float>(anim.SheetWidth));
                    // const float v1 = v0 + (static_cast<float>(anim.FrameHeight)       / static_cast<float>(anim.SheetHeight));

                    // Update SpriteRenderer2D
                    sprite->TextureId = anim.TextureId;
                    sprite->Width = anim.FrameWidth;
                    sprite->Height = anim.FrameHeight;
                    sprite->Tiling = Vector2D{
                        static_cast<float>(anim.FrameWidth)  / static_cast<float>(anim.SheetWidth),
                        static_cast<float>(anim.FrameHeight) / static_cast<float>(anim.SheetHeight)
                    };
                    sprite->Offset = Vector2D{ u0, v0 };
                }
            }
        );
    }

}
