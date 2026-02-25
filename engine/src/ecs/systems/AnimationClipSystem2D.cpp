/* Start Header *****************************************************************/
/*!
\file   AnimationClipSystem2D.cpp
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Updates frame timing, sprite data, and per-frame metadata for 2D animations.
*/
/* End Header *******************************************************************/

#include "ecs/systems/AnimationClipSystem2D.h"
#include "animation/AnimationAssetManager.h"
#include "core/messaging/MessageSystem.h"
#include "ecs/Components.h"
#include "ecs/StringTable.h"
#include "ecs/events/EventComponents.h"
#include "graphics/SpriteSheetUtils.h"
#include "services/ResourceManager.h"
#include "services/TimeSystem.h"
#include <unordered_map>
#include <vector>

namespace ECS {
    namespace {
        /**
         * @brief Represents the result of advancing the animation frame, including the new local frame index and whether the frame has changed.
         */
        struct FrameResult {
            int LocalFrame = 0;         // The new local frame index after advancing based on delta time and frame durations
            bool FrameChanged = false;  // Whether the frame index has changed as a result of advancing (used to determine if we need to update sprite data, trigger events, etc.)
        };

        /**
         * @brief Calculates the total number of frames (windows) in the animation clip based on the sprite sheet data and 
         * frame count information.
         * 
         * The calculation considers whether the clip uses a specific row for animation frames, the total frame count, and 
         * the dimensions of the sprite sheet to determine how many frames are available for animation.
         * 
         * @param clip The AnimationClip2DData containing the sprite sheet information and frame count data.
         * @return The total number of frames (windows) in the animation clip that can be used for playback.
         */
        int GetWindowCount(const Animation::AnimationClip2DData& clip) {
            if (clip.SpriteSheet.UseRow && clip.SpriteSheet.FrameLength > 0) {
                return clip.SpriteSheet.FrameLength;
            }
            if (clip.SpriteSheet.FrameCount > 0) {
                return clip.SpriteSheet.FrameCount;
            }

            // Calculate based on sprite sheet dimensions and frame size if frame count is not explicitly provided
            const int cols = clip.SpriteSheet.FrameWidth > 0 ? clip.SpriteSheet.SheetWidth / clip.SpriteSheet.FrameWidth : 0;
            const int rows = clip.SpriteSheet.FrameHeight > 0 ? clip.SpriteSheet.SheetHeight / clip.SpriteSheet.FrameHeight : 0;
            return std::max(0, cols * rows - clip.SpriteSheet.StartFrame);
        }

        /**
         * @brief Advances the animation frame based on the elapsed time (delta time) and the frame durations specified in 
         * the animation clip data.
         * 
         * The function updates the time accumulator with the delta time and checks if it exceeds the duration of the current frame. 
         * 
         * If it does, it advances to the next frame and resets the time accumulator accordingly.
         * 
         * The function also handles looping behavior based on the provided loop flag, ensuring that the animation cycles back to the 
         * beginning if looping is enabled, or stops at the last frame if looping is disabled.
         * 
         * @param dt The delta time (in seconds) since the last update, used to determine how much to advance the animation frame.
         * @param clip The AnimationClip2DData containing the frame durations and sprite sheet information for the animation clip being played.
         * @param timeAccumulator A reference to the time accumulator that tracks the elapsed time for the current frame, which will be updated by this function.
         * @param currentFrame The current local frame index before advancing, used to determine which frame duration to check against the time accumulator.
         * @param loop A boolean flag indicating whether the animation should loop back to the beginning when it reaches the end of the frames.
         * @return A FrameResult structure containing the updated local frame index and a flag indicating whether the frame has changed.
         */
        FrameResult AdvanceFrame(float dt, const Animation::AnimationClip2DData& clip,
            float& timeAccumulator, int currentFrame, bool loop) {
            FrameResult result{ currentFrame, false };

            // Determine the total number of frames in the clip to know when to loop or stop
            const int frameCount = GetWindowCount(clip);
            if (frameCount <= 0) {
                return result;
            }

            // If specific frame durations are provided, use them to advance frames; otherwise, use the default FPS from the sprite sheet data
            if (!clip.FrameDurations.empty() && currentFrame < static_cast<int>(clip.FrameDurations.size())) {
                float frameTime = clip.FrameDurations[currentFrame];
                timeAccumulator += dt;

                while (timeAccumulator >= frameTime && frameTime > 0.0f) {
                    timeAccumulator -= frameTime;
                    result.LocalFrame++;
                    result.FrameChanged = true;
                
                    if (result.LocalFrame >= frameCount) {
                        if (loop) {
                            result.LocalFrame = 0;
                        } else {
                            result.LocalFrame = frameCount - 1;
                            break;
                        }
                    }
                
                    if (result.LocalFrame < static_cast<int>(clip.FrameDurations.size())) {
                        frameTime = clip.FrameDurations[result.LocalFrame];
                    }
                }
                return result;
            }

            // If no specific frame durations are provided, use the default frames per second from the sprite sheet to calculate frame time
            const float frameTime = 1.0f / std::max(clip.SpriteSheet.FramesPerSecond, 0.001f);
            timeAccumulator += dt;

            while (timeAccumulator >= frameTime) {
                timeAccumulator -= frameTime;
                result.LocalFrame++;
                result.FrameChanged = true;
            
                if (result.LocalFrame >= frameCount) {
                    if (loop) {
                        result.LocalFrame = 0;
                    } else {
                        result.LocalFrame = frameCount - 1;
                        break;
                    }
                }
            }
            return result;
        }

        /**
         * @brief Pushes an animation event into the appropriate event buffer for the entity specified in the event data. 
         * The function checks if the buffer for the entity has space to accommodate the new event, and if so, it adds the 
         * event to the buffer and increments the count of events in that buffer.
         * 
         * The event data is converted from the Messaging::AnimationEvent2D format to the ECS::Events::AnimationEvent2D format 
         * before being stored in the buffer. 
         * 
         * If the buffer is full (i.e., it has reached the maximum number of events defined by kMaxEventBufferSize), the function 
         * will not add the new event and will return the current count of events in the buffer without modification.
         * 
         * @param outBuffers A reference to the unordered map that holds the animation event buffers for each entity, where the key 
         *  is the entity ID and the value is the AnimationEventBuffer2D that contains the events for that entity.
         * 
         * @param evt The animation event data in the Messaging::AnimationEvent2D format that needs to be pushed into the appropriate 
         *  event buffer for the entity specified in the event data.
         * 
         * @return The new count of events in the buffer for the entity after attempting to push the new event. 
         * If the buffer was full and the event was not added, this will return the current count without modification.
         */
        uint32_t PushEvent(std::unordered_map<uint32_t, ECS::Events::AnimationEventBuffer2D>& outBuffers,
            const Messaging::AnimationEvent2D& evt) {
            auto& buffer = outBuffers[evt.EntityId];
            
            // Check if the buffer has space for a new event before adding.
            // If it's full, we skip adding this event to prevent overflow.
            if (buffer.Count >= ECS::Events::kMaxEventBufferSize) {
                return buffer.Count;
            }
            
            ECS::Events::AnimationEvent2D entry{};
            entry.Type = static_cast<uint8_t>(evt.EventType);
            entry.EntityId = evt.EntityId;
            entry.LocalFrame = evt.LocalFrame;
            entry.AbsoluteFrame = evt.AbsoluteFrame;
            entry.WindowStart = evt.WindowStart;
            entry.WindowCount = evt.WindowCount;
            entry.Looping = evt.Looping;
            entry.Playing = evt.Playing;
            entry.NotifyNameId = evt.NotifyNameId;
            buffer.Events[buffer.Count++] = entry;
            return buffer.Count;
        }
    }

    SystemMetadata AnimationClipSystem2D::GetMetadata() const {
        ComponentAccessBuilder builder("AnimationClip2D");
        builder.ReadComponent<Components::AnimationController2D>();
        builder.ReadComponent<Components::AnimationRuntime2D>();
        builder.ReadComponent<Components::SpriteRenderer2D>();
        builder.WriteComponent<Components::AnimationState2D>();
        builder.WriteComponent<Components::AnimationBlend2D>();
        builder.WriteComponent<Components::AnimationHitboxBuffer2D>();
        builder.WriteComponent<Components::AnimationAttachmentBuffer2D>();
        builder.SetExecutionOrder(190);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    void AnimationClipSystem2D::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        std::vector<Entity> addState;
        std::vector<Entity> addBlend;
        std::vector<Entity> addHitboxes;
        std::vector<Entity> addAttachments;
        std::unordered_map<uint32_t, ECS::Events::AnimationEventBuffer2D> eventBuffers;

        world.Each<Components::AnimationController2D>([&](Entity entity, Components::AnimationController2D&) {
            if (!world.Has<Components::AnimationState2D>(entity)) {
                addState.push_back(entity);
            }
            if (!world.Has<Components::AnimationBlend2D>(entity)) {
                addBlend.push_back(entity);
            }
            if (!world.Has<Components::AnimationHitboxBuffer2D>(entity)) {
                addHitboxes.push_back(entity);
            }
            if (!world.Has<Components::AnimationAttachmentBuffer2D>(entity)) {
                addAttachments.push_back(entity);
            }
        });

        for (Entity entity : addState) {
            world.Add<Components::AnimationState2D>(entity); // Defer Add to avoid archetype mutation mid-iteration.
        }
        for (Entity entity : addBlend) {
            world.Add<Components::AnimationBlend2D>(entity); // Defer Add to avoid archetype mutation mid-iteration.
        }
        for (Entity entity : addHitboxes) {
            world.Add<Components::AnimationHitboxBuffer2D>(entity); // Defer Add to avoid archetype mutation mid-iteration.
        }
        for (Entity entity : addAttachments) {
            world.Add<Components::AnimationAttachmentBuffer2D>(entity); // Defer Add to avoid archetype mutation mid-iteration.
        }

        world.Each<Components::AnimationController2D, Components::AnimationRuntime2D, Components::AnimationState2D>(
            [&](Entity entity, Components::AnimationController2D& controllerComp,
                Components::AnimationRuntime2D& runtime, Components::AnimationState2D& state) {
                if (!world.IsActiveInHierarchy(entity)) {
                    return;
                }

                const auto* controller = Animation::AnimationAssetManager::Get().FindController(controllerComp.ControllerAssetId);
                if (!controller || controller->States.empty()) {
                    return;
                }

                if (runtime.CurrentState < 0 || runtime.CurrentState >= static_cast<int>(controller->States.size())) {
                    return;
                }

                const auto& currentState = controller->States[runtime.CurrentState];
                const uint32_t clipId = Animation::AnimationAssetManager::Get().GetOrLoadClip(currentState.ClipPath);
                const auto* clip = Animation::AnimationAssetManager::Get().FindClip(clipId);
                if (!clip) {
                    return;
                }

                const int windowCount = GetWindowCount(*clip);
                if (state.CurrentFrame < 0 || state.CurrentFrame >= windowCount) {
                    state.CurrentFrame = 0;
                }

                const int prevFrame = state.CurrentFrame;
                FrameResult frameResult = AdvanceFrame(dt, *clip, state.TimeAccumulator, state.CurrentFrame, currentState.Loop);
                state.CurrentFrame = frameResult.LocalFrame;

                const int absoluteFrame = clip->SpriteSheet.StartFrame + state.CurrentFrame;
                const glm::vec4 uv = SpriteSheetUtils::ComputeUV(
                    absoluteFrame,
                    clip->SpriteSheet.FrameWidth,
                    clip->SpriteSheet.FrameHeight,
                    clip->SpriteSheet.SheetWidth,
                    clip->SpriteSheet.SheetHeight);

                if (auto* sprite = world.TryGet<Components::SpriteRenderer2D>(entity)) {
                    if (!clip->SpriteSheet.TexturePath.empty()) {
                        if (auto tex = RM.Get<Texture>(clip->SpriteSheet.TexturePath)) {
                            sprite->TextureId = static_cast<uint32_t>(tex->ID());
                            sprite->Width = clip->SpriteSheet.FrameWidth;
                            sprite->Height = clip->SpriteSheet.FrameHeight;
                        }
                    }
                    if (!clip->SpriteSheet.NormalTexturePath.empty()) {
                        if (auto normal = RM.Get<Texture>(clip->SpriteSheet.NormalTexturePath)) {
                            sprite->NormalTextureId = static_cast<uint32_t>(normal->ID());
                        }
                    }
                    sprite->Tiling = Vector2D{ uv.z - uv.x, uv.w - uv.y };
                    sprite->Offset = Vector2D{ uv.x, uv.y };
                }

                if (auto* hitboxes = world.TryGet<Components::AnimationHitboxBuffer2D>(entity)) {
                    hitboxes->Count = 0;
                    if (state.CurrentFrame < static_cast<int>(clip->Frames.size())) {
                        const auto& frameData = clip->Frames[state.CurrentFrame];
                        for (const auto& hb : frameData.Hitboxes) {
                            if (hitboxes->Count >= Components::kAnimationMaxHitboxes) {
                                break;
                            }
                            auto& dst = hitboxes->Hitboxes[hitboxes->Count++];
                            dst.NameId = ECS::StringTable::Intern(hb.Name);
                            dst.Offset = hb.Offset;
                            dst.Size = hb.Size;
                            dst.Color = hb.Color;
                        }
                    }
                }

                if (auto* attachments = world.TryGet<Components::AnimationAttachmentBuffer2D>(entity)) {
                    attachments->Count = 0;
                    if (state.CurrentFrame < static_cast<int>(clip->Frames.size())) {
                        const auto& frameData = clip->Frames[state.CurrentFrame];
                        for (const auto& attach : frameData.Attachments) {
                            if (attachments->Count >= Components::kAnimationMaxAttachments) {
                                break;
                            }
                            auto& dst = attachments->Attachments[attachments->Count++];
                            dst.NameId = ECS::StringTable::Intern(attach.Name);
                            dst.Offset = attach.Offset;
                        }
                    }
                }

                if (state.CurrentFrame < static_cast<int>(clip->Frames.size())) {
                    const auto& frameData = clip->Frames[state.CurrentFrame];
                    if (auto* transform = world.TryGet<Components::LocalTransform>(entity)) {
                        transform->Position.X += frameData.RootMotion.X;
                        transform->Position.Y += frameData.RootMotion.Y;
                    }
                }

                if (auto* attachments = world.TryGet<Components::AnimationAttachmentBuffer2D>(entity)) {
                    world.ForChildren(entity, [&](Entity child) {
                        if (!world.Has<Components::Name>(child)) {
                            return;
                        }
                        const auto& name = world.Get<Components::Name>(child);
                        for (uint32_t i = 0; i < attachments->Count; ++i) {
                            if (attachments->Attachments[i].NameId == name.Value) {
                                if (auto* childTransform = world.TryGet<Components::LocalTransform>(child)) {
                                    childTransform->Position.X = attachments->Attachments[i].Offset.X;
                                    childTransform->Position.Y = attachments->Attachments[i].Offset.Y;
                                }
                            }
                        }
                    });
                }

                if (frameResult.FrameChanged && state.CurrentFrame < static_cast<int>(clip->Frames.size())) {
                    const auto& frameData = clip->Frames[state.CurrentFrame];
                    for (const auto& notify : frameData.Notifies) {
                        const uint32_t nameId = ECS::StringTable::Intern(notify.Name);
                        Messaging::AnimationEvent2D evt{
                            Messaging::AnimationEvent2D::Type::Notify,
                            entity.Index,
                            state.CurrentFrame,
                            absoluteFrame,
                            clip->SpriteSheet.StartFrame,
                            windowCount,
                            currentState.Loop,
                            runtime.Playing,
                            nameId
                        };
                        Messaging::MessageSystem::Notify(evt);
                        PushEvent(eventBuffers, evt);
                    }
                }

                if (prevFrame != state.CurrentFrame) {
                    Messaging::AnimationEvent2D evt{
                        Messaging::AnimationEvent2D::Type::FrameChanged,
                        entity.Index,
                        state.CurrentFrame,
                        absoluteFrame,
                        clip->SpriteSheet.StartFrame,
                        windowCount,
                        currentState.Loop,
                        runtime.Playing,
                        0
                    };
                    Messaging::MessageSystem::Notify(evt);
                    PushEvent(eventBuffers, evt);
                }

                if (runtime.NextState >= 0 && runtime.BlendDuration > 0.0f) {
                    runtime.NextStateTime += dt;
                    const auto& nextState = controller->States[runtime.NextState];
                    const uint32_t nextClipId = Animation::AnimationAssetManager::Get().GetOrLoadClip(nextState.ClipPath);
                    const auto* nextClip = Animation::AnimationAssetManager::Get().FindClip(nextClipId);
                    if (nextClip) {
                        FrameResult nextFrameResult = AdvanceFrame(dt, *nextClip, runtime.NextStateTime, 0, nextState.Loop);
                        const int nextAbsolute = nextClip->SpriteSheet.StartFrame + nextFrameResult.LocalFrame;
                        const glm::vec4 nextUv = SpriteSheetUtils::ComputeUV(
                            nextAbsolute,
                            nextClip->SpriteSheet.FrameWidth,
                            nextClip->SpriteSheet.FrameHeight,
                            nextClip->SpriteSheet.SheetWidth,
                            nextClip->SpriteSheet.SheetHeight);

                        if (auto* blend = world.TryGet<Components::AnimationBlend2D>(entity)) {
                            if (!nextClip->SpriteSheet.TexturePath.empty()) {
                                if (auto tex = RM.Get<Texture>(nextClip->SpriteSheet.TexturePath)) {
                                    blend->TextureId = static_cast<uint32_t>(tex->ID());
                                    blend->Width = nextClip->SpriteSheet.FrameWidth;
                                    blend->Height = nextClip->SpriteSheet.FrameHeight;
                                }
                            }
                            if (!nextClip->SpriteSheet.NormalTexturePath.empty()) {
                                if (auto normal = RM.Get<Texture>(nextClip->SpriteSheet.NormalTexturePath)) {
                                    blend->NormalTextureId = static_cast<uint32_t>(normal->ID());
                                }
                            }
                            blend->Tiling = Vector2D{ nextUv.z - nextUv.x, nextUv.w - nextUv.y };
                            blend->Offset = Vector2D{ nextUv.x, nextUv.y };
                            blend->Alpha = runtime.BlendAlpha;
                        }
                    }
                } else if (auto* blend = world.TryGet<Components::AnimationBlend2D>(entity)) {
                    blend->Alpha = 0.0f;
                }
            }
        );

        for (const auto& [entityId, buffer] : eventBuffers) {
            const Entity entity = world.Resolve(entityId);
            if (!world.IsAlive(entity)) {
                continue;
            }
            if (world.Has<ECS::Events::AnimationEventBuffer2D>(entity)) {
                world.Set<ECS::Events::AnimationEventBuffer2D>(entity, buffer);
            } else {
                world.Add<ECS::Events::AnimationEventBuffer2D>(entity, buffer);
            }
        }
    }
}
