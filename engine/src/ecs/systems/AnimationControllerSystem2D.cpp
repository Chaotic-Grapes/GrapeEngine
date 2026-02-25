/* Start Header *****************************************************************/
/*!
\file   AnimationControllerSystem2D.cpp
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Implements the controller-driven animation state machine for 2D sprites.
*/
/* End Header *******************************************************************/

#include "ecs/systems/AnimationControllerSystem2D.h"
#include "animation/AnimationAssetManager.h"
#include "ecs/Components.h"
#include "ecs/StringTable.h"
#include "services/TimeSystem.h"
#include "core/Logger.h"
#include <vector>

namespace ECS {
    namespace {
        /**
         * @brief Evaluates the conditions for transitioning between animation states based on the parameters 
         * defined in the animation controller and the current values of those parameters in the entity's AnimationParameters2D component.
         * 
         * The function iterates through the parameters defined in the controller to find the one that matches the condition's parameter name. 
         * Once found, it checks the type of the parameter (bool, int, or float) and compares the current value of that parameter against the 
         * value specified in the condition using the operator defined in the condition.
         * 
         * If the parameter is not found or if the type does not match, the function returns false, indicating that the condition is not met.
         * 
         * @param cond The AnimationTransitionCondition2D that specifies the parameter name, comparison operator, and value to compare against.
         * @param params The AnimationParameters2D component of the entity, which contains the current values of all animation parameters for that entity.
         * @param controller The AnimationController2DData that defines the parameters available in the animation controller, used to determine the 
         * type of the parameter being evaluated.
         * 
         * @return true if the condition is met based on the current parameter values and the condition's operator and value; false otherwise.
         */
        float ComputeClipDuration(const Animation::AnimationClip2DData& clip) {
            // If specific frame durations are provided in the clip data, sum them up to get the total duration of the clip.
            if (!clip.FrameDurations.empty()) {
                float sum = 0.0f;

                // Sum up the durations of each frame to get the total duration of the clip.
                for (float d : clip.FrameDurations) {
                    sum += d;
                }
                return sum;
            }

            // If no specific frame durations are provided, 
            // calculate the duration based on the number of frames and the frames per second defined in the sprite sheet data.
            int count = clip.SpriteSheet.FrameCount;
            if (clip.SpriteSheet.UseRow && clip.SpriteSheet.FrameLength > 0) {
                count = clip.SpriteSheet.FrameLength;
            }
            if (count <= 0) {
                return 0.0f;
            }
            return count / std::max(clip.SpriteSheet.FramesPerSecond, 0.001f);
        }

        /**
         * @brief Advances the animation frame based on the elapsed time (delta time) and the frame durations specified in the animation clip data.
         * 
         * The function updates the time accumulator with the delta time and checks if it exceeds the duration of the current frame. 
         * 
         * If it does, it advances to the next frame and resets the time accumulator accordingly.
         * 
         * The function also handles looping behavior based on the provided loop flag, ensuring that the animation cycles back to the beginning 
         * if looping is enabled, or stops at the last frame if looping is disabled.
         * 
         * @param dt The delta time (in seconds) since the last update, used to determine how much to advance the animation frame.
         * @param clip The AnimationClip2DData containing the frame durations and sprite sheet information for the animation clip being played.
         * @param timeAccumulator A reference to the time accumulator that tracks the elapsed time for the current frame, which will be updated by this function.
         * @param currentFrame The current local frame index before advancing, used to determine which frame duration to check against the time accumulator.
         * @param loop A boolean flag indicating whether the animation should loop back to the beginning when it reaches the end of the frames.
         * @return A FrameResult structure containing the updated local frame index and a flag indicating whether the frame has changed.
         */
        bool EvaluateCondition(const Animation::AnimationTransitionCondition2D& cond,
            const Components::AnimationParameters2D& params,
            const Animation::AnimationController2DData& controller) {

            // Iterate through the parameters defined in the controller to find the one that matches the condition's parameter name.
            for (size_t i = 0; i < controller.Parameters.size(); ++i) {
                if (controller.Parameters[i].Name != cond.Param) {
                    continue;
                }

                const auto& def = controller.Parameters[i];
                if (def.Type == Animation::ParamType::Bool && i < params.BoolValues.size()) {
                    const bool value = params.BoolValues[i];
                    const bool target = cond.Value != 0.0f;
                    return (cond.Op == Animation::CompareOp::Equal) ? (value == target) : (value != target);
                }

                if (def.Type == Animation::ParamType::Int && i < params.IntValues.size()) {
                    const int value = params.IntValues[i];
                    const int target = static_cast<int>(cond.Value);

                    if (cond.Op == Animation::CompareOp::Equal) return value == target;
                    if (cond.Op == Animation::CompareOp::NotEqual) return value != target;
                    if (cond.Op == Animation::CompareOp::Less) return value < target;
                    if (cond.Op == Animation::CompareOp::Greater) return value > target;
                }
                if (def.Type == Animation::ParamType::Float && i < params.FloatValues.size()) {
                    const float value = params.FloatValues[i];
                    
                    if (cond.Op == Animation::CompareOp::Equal) return value == cond.Value;
                    if (cond.Op == Animation::CompareOp::NotEqual) return value != cond.Value;
                    if (cond.Op == Animation::CompareOp::Less) return value < cond.Value;
                    if (cond.Op == Animation::CompareOp::Greater) return value > cond.Value;
                }
                break;
            }
            return false;
        }
    }

    SystemMetadata AnimationControllerSystem2D::GetMetadata() const {
        ComponentAccessBuilder builder("AnimationController2D");
        builder.ReadComponent<Components::AnimationController2D>();
        builder.ReadComponent<Components::AnimationParameters2D>();
        builder.WriteComponent<Components::AnimationRuntime2D>();
        builder.SetExecutionOrder(180);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    void AnimationControllerSystem2D::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        std::vector<Entity> addRuntime;
        std::vector<Entity> addParams;

        world.Each<Components::AnimationController2D>([&](Entity entity, Components::AnimationController2D&) {
            if (!world.Has<Components::AnimationRuntime2D>(entity)) {
                addRuntime.push_back(entity);
            }
            if (!world.Has<Components::AnimationParameters2D>(entity)) {
                addParams.push_back(entity);
            }
        });

        for (Entity entity : addRuntime) {
            world.Add<Components::AnimationRuntime2D>(entity); // Defer Add to avoid archetype mutation mid-iteration.
        }
        for (Entity entity : addParams) {
            world.Add<Components::AnimationParameters2D>(entity); // Defer Add to avoid archetype mutation mid-iteration.
        }

        world.Each<Components::AnimationController2D, Components::AnimationRuntime2D, Components::AnimationParameters2D>(
            [&](Entity entity, Components::AnimationController2D& controllerComp,
                Components::AnimationRuntime2D& runtime, Components::AnimationParameters2D& params) {
                if (!world.IsActiveInHierarchy(entity)) {
                    return;
                }

                std::string controllerPath = ECS::StringTable::Resolve(controllerComp.ControllerPath);
                if (controllerComp.ControllerAssetId == 0 && !controllerPath.empty()) {
                    controllerComp.ControllerAssetId = Animation::AnimationAssetManager::Get().GetOrLoadController(controllerPath);
                }

                const auto* controller = Animation::AnimationAssetManager::Get().FindController(controllerComp.ControllerAssetId);
                if (!controller) {
                    return;
                }

                if (params.BoolCount == 0 && params.IntCount == 0 && params.FloatCount == 0) {
                    params.BoolCount = static_cast<uint8_t>(std::min(controller->Parameters.size(), params.BoolValues.size()));
                    params.IntCount = static_cast<uint8_t>(std::min(controller->Parameters.size(), params.IntValues.size()));
                    params.FloatCount = static_cast<uint8_t>(std::min(controller->Parameters.size(), params.FloatValues.size()));
                    for (size_t i = 0; i < controller->Parameters.size() && i < params.BoolValues.size(); ++i) {
                        const auto& def = controller->Parameters[i];
                        if (def.Type == Animation::ParamType::Bool) {
                            params.BoolValues[i] = def.DefaultBool;
                        } else if (def.Type == Animation::ParamType::Int) {
                            params.IntValues[i] = def.DefaultInt;
                        } else {
                            params.FloatValues[i] = def.DefaultFloat;
                        }
                    }
                }

                if (runtime.CurrentState < 0 || runtime.CurrentState >= static_cast<int>(controller->States.size())) {
                    for (size_t i = 0; i < controller->States.size(); ++i) {
                        if (controller->States[i].Name == controller->EntryState) {
                            runtime.CurrentState = static_cast<int>(i);
                            runtime.StateTime = 0.0f;
                            break;
                        }
                    }
                }

                if (!runtime.Playing) {
                    return;
                }

                runtime.StateTime += dt;

                if (runtime.NextState >= 0) {
                    if (runtime.BlendDuration > 0.0f) {
                        runtime.BlendAlpha = std::min(1.0f, runtime.BlendAlpha + dt / runtime.BlendDuration);
                    } else {
                        runtime.BlendAlpha = 1.0f;
                    }

                    if (runtime.BlendAlpha >= 1.0f) {
                        runtime.CurrentState = runtime.NextState;
                        runtime.NextState = -1;
                        runtime.BlendAlpha = 0.0f; // Reset so next transition starts cleanly.
                        runtime.StateTime = 0.0f;
                        runtime.NextStateTime = 0.0f;
                    }
                    return;
                }

                const auto& currentState = controller->States[runtime.CurrentState];
                float clipDuration = 0.0f;
                if (!currentState.ClipPath.empty()) {
                    const uint32_t clipId = Animation::AnimationAssetManager::Get().GetOrLoadClip(currentState.ClipPath);
                    if (const auto* clip = Animation::AnimationAssetManager::Get().FindClip(clipId)) {
                        clipDuration = ComputeClipDuration(*clip);
                    }
                }

                for (const auto& transition : controller->Transitions) {
                    if (transition.FromState != currentState.Name) {
                        continue;
                    }

                    bool allConditionsMet = true;
                    for (const auto& cond : transition.Conditions) {
                        if (!EvaluateCondition(cond, params, *controller)) {
                            allConditionsMet = false;
                            break;
                        }
                    }
                    if (!allConditionsMet) {
                        continue;
                    }

                    if (transition.ExitTime > 0.0f && clipDuration > 0.0f) {
                        const float normalizedTime = runtime.StateTime / clipDuration;
                        if (normalizedTime < transition.ExitTime) {
                            continue;
                        }
                    }

                    for (size_t i = 0; i < controller->States.size(); ++i) {
                        if (controller->States[i].Name == transition.ToState) {
                            runtime.NextState = static_cast<int>(i);
                            runtime.BlendDuration = transition.Duration;
                            runtime.BlendAlpha = 0.0f;
                            runtime.NextStateTime = 0.0f;
                            break;
                        }
                    }
                    break;
                }
            }
        );
    }
}
