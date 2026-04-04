/* Start Header *****************************************************************/
/*!
\file   GUIInputSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Definition of the GUIInputSystem class for processing input interactions with GUI
elements. Provides functionality for handling mouse events, hover states, and
pointer capture within the GUI system.
*/
/* End Header *******************************************************************/

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <glm/glm.hpp>
#include "ecs/Components.h"
#include "ecs/systems/GUIInputSystem.h"
#include "ecs/systems/GUIRenderUtils.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/renderer.hpp"
#include "services/Input.h"
#include "services/TimeSystem.h"

namespace ECS {
    namespace {
        enum class NavigationDirection : int {
            Left = 0,
            Right = 1,
            Up = 2,
            Down = 3
        };

        struct NavigationCandidate {
            Entity Item = NULL_ENTITY;
            int16_t ZOrder = std::numeric_limits<int16_t>::min();
            Vector2D Center{};
            bool IsSlider = false;
        };

        struct SliderInteractionBasis {
            Vector2D Center{};
            Vector2D PrimaryAxis{};
            Vector2D SecondaryAxis{};
            float PrimaryLength = 0.0f;
            float SecondaryLength = 0.0f;
            bool Valid = false;
        };

        float Dot2(const Vector2D& a, const Vector2D& b) {
            return a.X * b.X + a.Y * b.Y;
        }

        Vector2D Sub2(const Vector2D& a, const Vector2D& b) {
            return { a.X - b.X, a.Y - b.Y };
        }

        float Length2(const Vector2D& v) {
            return std::sqrt(v.X * v.X + v.Y * v.Y);
        }

        /**
         * @brief Return the normalized axis vector used for directional navigation.
         * @param direction Requested direction.
         * @return Unit direction vector in screen-space coordinates.
         * @complexity O(1).
         */
        Vector2D DirectionVector(const NavigationDirection direction) {
            switch (direction) {
            case NavigationDirection::Left:
                return { -1.0f, 0.0f };
            case NavigationDirection::Right:
                return { 1.0f, 0.0f };
            case NavigationDirection::Up:
                return { 0.0f, -1.0f };
            case NavigationDirection::Down:
                return { 0.0f, 1.0f };
            }
            return { 0.0f, 0.0f };
        }

        /**
         * @brief Resolve explicit GUINavigation override target for a direction.
         * @param world ECS world used for component lookup.
         * @param source Current focused entity.
         * @param direction Navigation direction.
         * @return Target entity when override exists and is alive; NULL_ENTITY otherwise.
         * @complexity O(1).
         */
        Entity ResolveDirectionalOverride(const World& world, const Entity source, const NavigationDirection direction) {
            if (!world.Has<Components::GUINavigation>(source)) {
                return NULL_ENTITY;
            }

            const auto& nav = world.Get<Components::GUINavigation>(source);
            if (!nav.Enabled) {
                return NULL_ENTITY;
            }

            Entity overrideTarget = NULL_ENTITY;
            switch (direction) {
            case NavigationDirection::Left:
                overrideTarget = nav.Left;
                break;
            case NavigationDirection::Right:
                overrideTarget = nav.Right;
                break;
            case NavigationDirection::Up:
                overrideTarget = nav.Up;
                break;
            case NavigationDirection::Down:
                overrideTarget = nav.Down;
                break;
            }

            if (overrideTarget.IsNull() || !world.IsAlive(overrideTarget)) {
                return NULL_ENTITY;
            }

            return overrideTarget;
        }

        /**
         * @brief Compute a directional ranking score for focus candidates.
         * @param directionDir Unit vector for requested direction.
         * @param from Source center.
         * @param to Candidate center.
         * @param outScore Receives candidate score when candidate is directionally valid.
         * @return True when candidate lies in requested half-plane.
         * @note Lower scores are better.
         * @complexity O(1).
         */
        bool ComputeDirectionalScore(const Vector2D& directionDir,
                                     const Vector2D& from,
                                     const Vector2D& to,
                                     float& outScore) {
            const Vector2D delta = Sub2(to, from);
            const float forward = Dot2(delta, directionDir);
            if (forward <= 0.0001f) {
                return false;
            }

            const Vector2D perpendicular = { -directionDir.Y, directionDir.X };
            const float lateral = std::abs(Dot2(delta, perpendicular));

            // Bias towards forward progress while penalizing large lateral jumps.
            outScore = forward + lateral * 0.35f;
            return true;
        }

        /**
         * @brief Find first connected gamepad slot.
         * @return Gamepad index in [0, MAX_GAMEPADS) or -1 when unavailable.
         * @complexity O(MAX_GAMEPADS).
         */
        int FindConnectedGamepad() {
            for (int gamepad = 0; gamepad < MAX_GAMEPADS; ++gamepad) {
                if (Input::IsGamepadConnected(gamepad)) {
                    return gamepad;
                }
            }
            return -1;
        }

        Vector2D Rotate2D(const Vector2D& v, float radians) {
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            return {
                v.X * c - v.Y * s,
                v.X * s + v.Y * c
            };
        }

        bool PointInRect(const Vector2D& p, const Vector2D& pos, const Vector2D& size) {
            return p.X >= pos.X && p.Y >= pos.Y && p.X <= (pos.X + size.X) && p.Y <= (pos.Y + size.Y);
        }

        bool IsEntityInteractable(const World& world, Entity entity, const Components::GUIElement& element) {
            if (!world.IsActiveInHierarchy(entity) || !element.Visible) {
                return false;
            }
            if (world.Has<Components::GUIButton>(entity) && world.Get<Components::GUIButton>(entity).Disabled) {
                return false;
            }
            if (world.Has<Components::GUISlider>(entity)) {
                const auto& slider = world.Get<Components::GUISlider>(entity);
                if (slider.Disabled || slider.Max <= slider.Min) {
                    return false;
                }
            }
            return true;
        }

        bool IsHigherPriorityHit(Entity candidateEntity, int16_t candidateZ, Entity currentEntity, int16_t currentZ) {
            // Deterministic winner: higher Z first, then stable entity-id tie-break.
            if (candidateZ != currentZ) {
                return candidateZ > currentZ;
            }
            return candidateEntity > currentEntity;
        }

        float QuantizeAndClampSliderValue(float value, const Components::GUISlider& slider) {
            if (slider.Step > 0.0f) {
                value = slider.Min + std::round((value - slider.Min) / slider.Step) * slider.Step;
            }
            return std::clamp(value, slider.Min, slider.Max);
        }

        bool ProjectCursorToSliderBasis(const SliderInteractionBasis& basis,
                                        const Vector2D& cursor,
                                        float& outPrimaryCoord,
                                        float& outSecondaryCoord,
                                        float& outT) {
            if (!basis.Valid || basis.PrimaryLength <= 0.0001f || basis.SecondaryLength <= 0.0001f) {
                return false;
            }

            const Vector2D primaryDir = {
                basis.PrimaryAxis.X / basis.PrimaryLength,
                basis.PrimaryAxis.Y / basis.PrimaryLength
            };
            const Vector2D secondaryDir = {
                basis.SecondaryAxis.X / basis.SecondaryLength,
                basis.SecondaryAxis.Y / basis.SecondaryLength
            };

            const Vector2D primaryStart = {
                basis.Center.X - basis.PrimaryAxis.X * 0.5f,
                basis.Center.Y - basis.PrimaryAxis.Y * 0.5f
            };
            const Vector2D relStart = Sub2(cursor, primaryStart);
            const Vector2D relCenter = Sub2(cursor, basis.Center);

            // Measure cursor in slider-local coordinates so rotated/world sliders use one math path.
            outPrimaryCoord = Dot2(relStart, primaryDir);
            outSecondaryCoord = Dot2(relCenter, secondaryDir);
            outT = std::clamp(outPrimaryCoord / basis.PrimaryLength, 0.0f, 1.0f);
            return true;
        }

        bool IsCursorInsideSliderTrack(const SliderInteractionBasis& basis,
                                       const Vector2D& cursor,
                                       float& outPrimaryCoord,
                                       float& outT) {
            float secondaryCoord = 0.0f;
            if (!ProjectCursorToSliderBasis(basis, cursor, outPrimaryCoord, secondaryCoord, outT)) {
                return false;
            }

            const bool withinPrimary = outPrimaryCoord >= 0.0f && outPrimaryCoord <= basis.PrimaryLength;
            const bool withinSecondary = std::abs(secondaryCoord) <= (basis.SecondaryLength * 0.5f);
            return withinPrimary && withinSecondary;
        }
    }

    /**
     * @brief Reset all transient GUI input state for a fresh world/session.
     * @return void
     * @complexity O(1).
     */
    void GUIInputSystem::ResetState() {
        m_captureEntity = NULL_ENTITY;
        m_activeSlider = NULL_ENTITY;
        m_focusedEntity = NULL_ENTITY;
        m_sliderLastAxisCoord = 0.0f;
        m_sliderAxisValid = false;
        m_activeGamepad = -1;
        m_inputMode = InputMode::Pointer;
        m_navRepeatTimer = 0.0f;
        m_navHeldDirection = -1;
        m_sliderRepeatTimer = 0.0f;
        m_sliderHeldDirection = 0;
        m_lastMousePosition = { 0.0f, 0.0f };
        m_hasLastMousePosition = false;
    }

    /**
     * @brief Initialize per-world GUI input state.
     * @param world ECS world owning this system instance.
     * @return void
     * @complexity O(1).
     */
    void GUIInputSystem::OnCreate(World& world) {
        (void)world;
        ResetState();
    }

    /**
     * @brief Update GUI pointer and controller interaction state for one frame.
     * @param world ECS world containing GUI entities/components.
     * @return void
     * @note Mouse capture behavior remains unchanged; controller adds focus-driven interaction.
     * @complexity O(n) where n is count of entities with GUIElement+GUIInput.
     */
    void GUIInputSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());

        if (!m_captureEntity.IsNull() && !world.IsAlive(m_captureEntity)) {
            m_captureEntity = NULL_ENTITY;
        }
        if (!m_activeSlider.IsNull() && !world.IsAlive(m_activeSlider)) {
            m_activeSlider = NULL_ENTITY;
            m_sliderAxisValid = false;
        }
        if (!m_focusedEntity.IsNull() && !world.IsAlive(m_focusedEntity)) {
            m_focusedEntity = NULL_ENTITY;
        }

        if (m_activeGamepad < 0 || !Input::IsGamepadConnected(m_activeGamepad)) {
            m_activeGamepad = FindConnectedGamepad();
        }

        RendererSystem::GUIViewport viewport = renderer->GetGUIViewport();
        if (!viewport.Active || viewport.Size.X <= 0.0f || viewport.Size.Y <= 0.0f) {
            const Vector2D renderSize = renderer->GetRenderTargetSize();
            viewport.Origin = { 0.0f, 0.0f };
            viewport.Size = renderSize;
        }

        auto worldToScreen = [&](const Vector3D& worldPos, Vector2D& outScreen) {
            return renderer->WorldToScreen(world, worldPos, viewport.Origin, viewport.Size, outScreen);
        };

        auto buildSliderBasis = [&](Entity entity,
                                    const Components::GUIElement& element,
                                    const Components::GUISlider& slider,
                                    bool isWorldSpace,
                                    SliderInteractionBasis& outBasis) {
            outBasis = {};

            const float ySign = isWorldSpace ? -1.0f : 1.0f;
            const Vector2D padding = isWorldSpace
                ? Vector2D{ PixelsToUnits(slider.Padding.X), PixelsToUnits(slider.Padding.Y) }
                : Vector2D{
                    slider.Padding.X * (element.Size.X > 0.0f ? element.ResolvedSize.X / element.Size.X : 1.0f),
                    slider.Padding.Y * (element.Size.Y > 0.0f ? element.ResolvedSize.Y / element.Size.Y : 1.0f)
                };
            const Vector2D paddingOpposite = isWorldSpace
                ? Vector2D{ PixelsToUnits(slider.Padding.Z), PixelsToUnits(slider.Padding.W) }
                : Vector2D{
                    slider.Padding.Z * (element.Size.X > 0.0f ? element.ResolvedSize.X / element.Size.X : 1.0f),
                    slider.Padding.W * (element.Size.Y > 0.0f ? element.ResolvedSize.Y / element.Size.Y : 1.0f)
                };

            const Vector2D trackPos = {
                element.ContentPosition.X + padding.X,
                element.ContentPosition.Y + padding.Y * ySign
            };
            const Vector2D trackSize = {
                std::max(0.0f, element.ContentSize.X - padding.X - paddingOpposite.X),
                std::max(0.0f, element.ContentSize.Y - padding.Y - paddingOpposite.Y)
            };

            if (trackSize.X <= 0.0001f || trackSize.Y <= 0.0001f) {
                return;
            }

            const float rotationRadians = slider.RotationDegrees * (3.14159265358979323846f / 180.0f);
            const Vector2D basePrimary = slider.Horizontal
                ? Vector2D{ trackSize.X, 0.0f }
                : Vector2D{ 0.0f, trackSize.Y };
            const Vector2D baseSecondary = slider.Horizontal
                ? Vector2D{ 0.0f, trackSize.Y }
                : Vector2D{ trackSize.X, 0.0f };

            if (!isWorldSpace) {
                // Screen-space basis directly uses resolved GUI layout dimensions.
                outBasis.Center = {
                    trackPos.X + trackSize.X * 0.5f,
                    trackPos.Y + trackSize.Y * 0.5f
                };
                outBasis.PrimaryAxis = Rotate2D(basePrimary, rotationRadians);
                outBasis.SecondaryAxis = Rotate2D(baseSecondary, rotationRadians);
                outBasis.PrimaryLength = Length2(outBasis.PrimaryAxis);
                outBasis.SecondaryLength = Length2(outBasis.SecondaryAxis);
                outBasis.Valid = outBasis.PrimaryLength > 0.0001f && outBasis.SecondaryLength > 0.0001f;
                return;
            }

            float z = 0.0f;
            if (world.Has<Components::WorldTransform>(entity)) {
                z = world.Get<Components::WorldTransform>(entity).Matrix.m23;
            } else if (world.Has<Components::LocalTransform>(entity)) {
                z = world.Get<Components::LocalTransform>(entity).Position.Z;
            }

            const Vector2D worldCenter = {
                trackPos.X + trackSize.X * 0.5f,
                trackPos.Y + trackSize.Y * 0.5f
            };
            const Vector2D worldPrimary = Rotate2D(basePrimary, rotationRadians);
            const Vector2D worldSecondary = Rotate2D(baseSecondary, rotationRadians);

            const Vector3D worldCenter3{ worldCenter.X, worldCenter.Y, z };
            const Vector3D worldPrimaryEnd3{ worldCenter.X + worldPrimary.X * 0.5f, worldCenter.Y + worldPrimary.Y * 0.5f, z };
            const Vector3D worldSecondaryEnd3{ worldCenter.X + worldSecondary.X * 0.5f, worldCenter.Y + worldSecondary.Y * 0.5f, z };

            Vector2D screenCenter{}, screenPrimaryEnd{}, screenSecondaryEnd{};
            if (!worldToScreen(worldCenter3, screenCenter) ||
                !worldToScreen(worldPrimaryEnd3, screenPrimaryEnd) ||
                !worldToScreen(worldSecondaryEnd3, screenSecondaryEnd)) {
                return;
            }

            // World-space basis is projected to screen so hit-testing matches what is rendered.
            outBasis.Center = screenCenter;
            outBasis.PrimaryAxis = {
                (screenPrimaryEnd.X - screenCenter.X) * 2.0f,
                (screenPrimaryEnd.Y - screenCenter.Y) * 2.0f
            };
            outBasis.SecondaryAxis = {
                (screenSecondaryEnd.X - screenCenter.X) * 2.0f,
                (screenSecondaryEnd.Y - screenCenter.Y) * 2.0f
            };
            outBasis.PrimaryLength = Length2(outBasis.PrimaryAxis);
            outBasis.SecondaryLength = Length2(outBasis.SecondaryAxis);
            outBasis.Valid = outBasis.PrimaryLength > 0.0001f && outBasis.SecondaryLength > 0.0001f;
        };

        // Convert raw cursor position into GUI viewport space.
        double mouseX = 0.0;
        double mouseY = 0.0;
        Input::GetMousePosition(mouseX, mouseY);

        const float scaleX = std::max(0.0001f, viewport.DisplayScale.X);
        const float scaleY = std::max(0.0001f, viewport.DisplayScale.Y);

        const Vector2D mouseRaw = {
            static_cast<float>(mouseX),
            static_cast<float>(mouseY)
        };
        const Vector2D mouseScaled = {
            static_cast<float>(mouseX / scaleX),
            static_cast<float>(mouseY / scaleY)
        };
        const bool rawInViewport = PointInRect(mouseRaw, viewport.Origin, viewport.Size);
        const bool scaledInViewport = PointInRect(mouseScaled, viewport.Origin, viewport.Size);
        const bool hasDpiScale = (std::abs(scaleX - 1.0f) > 0.001f) || (std::abs(scaleY - 1.0f) > 0.001f);
        const Vector2D mouse = (rawInViewport != scaledInViewport)
            ? (rawInViewport ? mouseRaw : mouseScaled)
            : (hasDpiScale ? mouseScaled : mouseRaw);

        bool mouseMoved = false;
        if (m_hasLastMousePosition) {
            const Vector2D mouseDelta = Sub2(mouse, m_lastMousePosition);
            mouseMoved = std::abs(mouseDelta.X) > 0.5f || std::abs(mouseDelta.Y) > 0.5f;
        }
        m_lastMousePosition = mouse;
        m_hasLastMousePosition = true;

        // Snapshot mouse button state for this frame.
        const bool mouseDown = Input::IsMouseDown(MOUSE_LEFT);
        const bool mousePressed = Input::IsMousePressed(MOUSE_LEFT);
        const bool mouseReleased = Input::IsMouseUp(MOUSE_LEFT);

        if (mouseMoved || mousePressed || mouseReleased) {
            m_inputMode = InputMode::Pointer;
        }

        bool navigationTriggered = false;
        bool submitTriggered = false;
        bool cancelTriggered = false;
        bool sliderAdjustedByController = false;
        int navigationDirectionInt = -1;
        int sliderDirectionInt = 0;
        bool sliderCoarseStep = false;

        bool focusedSliderConsumesHorizontal = false;
        bool focusedSliderConsumesVertical = false;
        if (!m_focusedEntity.IsNull() && world.IsAlive(m_focusedEntity) && world.Has<Components::GUISlider>(m_focusedEntity)
            && world.Has<Components::GUIElement>(m_focusedEntity)) {
            const auto& focusedElement = world.Get<Components::GUIElement>(m_focusedEntity);
            const auto& focusedSlider = world.Get<Components::GUISlider>(m_focusedEntity);
            if (IsEntityInteractable(world, m_focusedEntity, focusedElement)
                && !focusedSlider.Disabled && focusedSlider.Max > focusedSlider.Min) {
                focusedSliderConsumesHorizontal = focusedSlider.Horizontal;
                focusedSliderConsumesVertical = !focusedSlider.Horizontal;
            }
        }

        if (m_activeGamepad >= 0) {
            const bool dpadLeftPressed = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_DPAD_LEFT);
            const bool dpadRightPressed = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_DPAD_RIGHT);
            const bool dpadUpPressed = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_DPAD_UP);
            const bool dpadDownPressed = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_DPAD_DOWN);

            const bool dpadLeftDown = Input::IsGamepadButtonDown(m_activeGamepad, GAMEPAD_BUTTON_DPAD_LEFT);
            const bool dpadRightDown = Input::IsGamepadButtonDown(m_activeGamepad, GAMEPAD_BUTTON_DPAD_RIGHT);
            const bool dpadUpDown = Input::IsGamepadButtonDown(m_activeGamepad, GAMEPAD_BUTTON_DPAD_UP);
            const bool dpadDownDown = Input::IsGamepadButtonDown(m_activeGamepad, GAMEPAD_BUTTON_DPAD_DOWN);

            const float leftX = Input::GetGamepadAxisWithDeadzone(m_activeGamepad, GAMEPAD_AXIS_LEFT_X, 0.35f);
            const float leftY = Input::GetGamepadAxisWithDeadzone(m_activeGamepad, GAMEPAD_AXIS_LEFT_Y, 0.35f);

            auto decodeDirectionalInput = [&](const float stickX, const float stickY) -> int {
                if (std::abs(stickX) < 0.45f && std::abs(stickY) < 0.45f) {
                    return -1;
                }
                if (std::abs(stickX) >= std::abs(stickY)) {
                    return stickX < 0.0f ? static_cast<int>(NavigationDirection::Left)
                                         : static_cast<int>(NavigationDirection::Right);
                }
                return stickY < 0.0f ? static_cast<int>(NavigationDirection::Up)
                                     : static_cast<int>(NavigationDirection::Down);
            };

            int heldDirection = decodeDirectionalInput(leftX, leftY);
            if (dpadLeftDown) {
                heldDirection = static_cast<int>(NavigationDirection::Left);
            } else if (dpadRightDown) {
                heldDirection = static_cast<int>(NavigationDirection::Right);
            } else if (dpadUpDown) {
                heldDirection = static_cast<int>(NavigationDirection::Up);
            } else if (dpadDownDown) {
                heldDirection = static_cast<int>(NavigationDirection::Down);
            }

            const bool horizontalNavigationHeld = heldDirection == static_cast<int>(NavigationDirection::Left)
                || heldDirection == static_cast<int>(NavigationDirection::Right);
            const bool verticalNavigationHeld = heldDirection == static_cast<int>(NavigationDirection::Up)
                || heldDirection == static_cast<int>(NavigationDirection::Down);
            if ((focusedSliderConsumesHorizontal && horizontalNavigationHeld)
                || (focusedSliderConsumesVertical && verticalNavigationHeld)) {
                heldDirection = -1;
            }

            const bool navLeftPressed = dpadLeftPressed && !focusedSliderConsumesHorizontal;
            const bool navRightPressed = dpadRightPressed && !focusedSliderConsumesHorizontal;
            const bool navUpPressed = dpadUpPressed && !focusedSliderConsumesVertical;
            const bool navDownPressed = dpadDownPressed && !focusedSliderConsumesVertical;
            const bool immediatePress = navLeftPressed || navRightPressed || navUpPressed || navDownPressed;

            if (heldDirection == -1) {
                m_navHeldDirection = -1;
                m_navRepeatTimer = 0.0f;
            } else if (m_navHeldDirection != heldDirection) {
                m_navHeldDirection = heldDirection;
                m_navRepeatTimer = m_navInitialRepeatDelay;
                navigationTriggered = true;
                navigationDirectionInt = heldDirection;
            } else {
                m_navRepeatTimer -= dt;
                if (m_navRepeatTimer <= 0.0f) {
                    m_navRepeatTimer = m_navRepeatInterval;
                    navigationTriggered = true;
                    navigationDirectionInt = heldDirection;
                }
            }

            if (immediatePress) {
                navigationTriggered = true;
                if (navLeftPressed) {
                    navigationDirectionInt = static_cast<int>(NavigationDirection::Left);
                } else if (navRightPressed) {
                    navigationDirectionInt = static_cast<int>(NavigationDirection::Right);
                } else if (navUpPressed) {
                    navigationDirectionInt = static_cast<int>(NavigationDirection::Up);
                } else if (navDownPressed) {
                    navigationDirectionInt = static_cast<int>(NavigationDirection::Down);
                }
                m_navHeldDirection = navigationDirectionInt;
                m_navRepeatTimer = m_navInitialRepeatDelay;
            }

            submitTriggered = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_A);
            cancelTriggered = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_B);

            const bool sliderHorizontal = focusedSliderConsumesHorizontal;
            const bool sliderVertical = focusedSliderConsumesVertical;
            const bool sliderDecPressed = (sliderHorizontal && dpadLeftPressed) || (sliderVertical && dpadDownPressed);
            const bool sliderIncPressed = (sliderHorizontal && dpadRightPressed) || (sliderVertical && dpadUpPressed);
            const bool sliderDecDown = (sliderHorizontal && dpadLeftDown) || (sliderVertical && dpadDownDown);
            const bool sliderIncDown = (sliderHorizontal && dpadRightDown) || (sliderVertical && dpadUpDown);
            const bool leftBumperPressed = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_LEFT_BUMPER);
            const bool rightBumperPressed = Input::IsGamepadButtonPressed(m_activeGamepad, GAMEPAD_BUTTON_RIGHT_BUMPER);

            const float sliderAxis = sliderHorizontal ? leftX : (sliderVertical ? -leftY : 0.0f);

            int sliderHeldDirection = 0;
            if (sliderDecDown || sliderAxis < -0.50f) {
                sliderHeldDirection = -1;
            } else if (sliderIncDown || sliderAxis > 0.50f) {
                sliderHeldDirection = 1;
            }

            if (sliderHeldDirection == 0) {
                m_sliderHeldDirection = 0;
                m_sliderRepeatTimer = 0.0f;
            } else if (m_sliderHeldDirection != sliderHeldDirection) {
                m_sliderHeldDirection = sliderHeldDirection;
                m_sliderRepeatTimer = m_sliderInitialRepeatDelay;
                sliderAdjustedByController = true;
                sliderDirectionInt = sliderHeldDirection;
            } else {
                m_sliderRepeatTimer -= dt;
                if (m_sliderRepeatTimer <= 0.0f) {
                    m_sliderRepeatTimer = m_sliderRepeatInterval;
                    sliderAdjustedByController = true;
                    sliderDirectionInt = sliderHeldDirection;
                }
            }

            if (sliderDecPressed || sliderIncPressed || leftBumperPressed || rightBumperPressed) {
                sliderAdjustedByController = true;
                if (sliderDecPressed || leftBumperPressed) {
                    sliderDirectionInt = -1;
                } else if (sliderIncPressed || rightBumperPressed) {
                    sliderDirectionInt = 1;
                }
                sliderCoarseStep = leftBumperPressed || rightBumperPressed;
                m_sliderHeldDirection = sliderDirectionInt;
                m_sliderRepeatTimer = m_sliderInitialRepeatDelay;
            }

            if (navigationTriggered || submitTriggered || cancelTriggered || sliderAdjustedByController) {
                m_inputMode = InputMode::Gamepad;
            }
        } else {
            m_navHeldDirection = -1;
            m_navRepeatTimer = 0.0f;
            m_sliderHeldDirection = 0;
            m_sliderRepeatTimer = 0.0f;
        }

        std::vector<NavigationCandidate> navigationCandidates;
        navigationCandidates.reserve(64);

        // Resolve top-most hovered element by z-order and stable entity tie-break.
        Entity topHovered = NULL_ENTITY;
        int16_t topHoveredZ = std::numeric_limits<int16_t>::min();
        world.Each<Components::GUIElement, Components::GUIInput>([&](Entity entity, Components::GUIElement& element, Components::GUIInput&) {
            if (!IsEntityInteractable(world, entity, element)) {
                return;
            }

            const bool isWorldSpace = (ResolveGUIRenderSpace(world, entity) == Components::GUIRenderSpace::World);
            bool hit = false;
            Vector2D center{};

            if (world.Has<Components::GUISlider>(entity)) {
                const auto& slider = world.Get<Components::GUISlider>(entity);
                SliderInteractionBasis basis;
                buildSliderBasis(entity, element, slider, isWorldSpace, basis);
                center = basis.Valid ? basis.Center : Vector2D{};

                float axisCoord = 0.0f;
                float tFromCursor = 0.0f;
                hit = IsCursorInsideSliderTrack(basis, mouse, axisCoord, tFromCursor);
            } else {
                const Vector2D pos = isWorldSpace ? element.ScreenPosition : element.ResolvedPosition;
                const Vector2D size = isWorldSpace ? element.ScreenSize : element.ResolvedSize;
                center = { pos.X + size.X * 0.5f, pos.Y + size.Y * 0.5f };
                hit = PointInRect(mouse, pos, size);
            }

            navigationCandidates.push_back(NavigationCandidate{
                entity,
                element.ZOrder,
                center,
                world.Has<Components::GUISlider>(entity)
            });

            if (!hit) {
                return;
            }

            if (topHovered.IsNull() || IsHigherPriorityHit(entity, element.ZOrder, topHovered, topHoveredZ)) {
                topHovered = entity;
                topHoveredZ = element.ZOrder;
            }
        });

        auto findCandidate = [&](const Entity target) -> const NavigationCandidate* {
            for (const NavigationCandidate& candidate : navigationCandidates) {
                if (candidate.Item == target) {
                    return &candidate;
                }
            }
            return nullptr;
        };

        if (!m_focusedEntity.IsNull() && !findCandidate(m_focusedEntity)) {
            m_focusedEntity = NULL_ENTITY;
        }

        if (m_focusedEntity.IsNull() && !topHovered.IsNull()) {
            m_focusedEntity = topHovered;
        }

        if (m_focusedEntity.IsNull() && !navigationCandidates.empty()) {
            const NavigationCandidate* best = &navigationCandidates.front();
            for (const NavigationCandidate& candidate : navigationCandidates) {
                if (IsHigherPriorityHit(candidate.Item, candidate.ZOrder, best->Item, best->ZOrder)) {
                    best = &candidate;
                }
            }
            m_focusedEntity = best->Item;
        }

        if (navigationTriggered && navigationDirectionInt >= 0 && !navigationCandidates.empty()) {
            const NavigationDirection navDirection = static_cast<NavigationDirection>(navigationDirectionInt);
            Entity nextFocus = NULL_ENTITY;

            if (!m_focusedEntity.IsNull()) {
                const Entity overrideEntity = ResolveDirectionalOverride(world, m_focusedEntity, navDirection);
                if (!overrideEntity.IsNull() && findCandidate(overrideEntity)) {
                    nextFocus = overrideEntity;
                }
            }

            const NavigationCandidate* focusedCandidate = findCandidate(m_focusedEntity);
            if (nextFocus.IsNull() && focusedCandidate) {
                const Vector2D directionVector = DirectionVector(navDirection);

                float bestScore = std::numeric_limits<float>::max();
                const NavigationCandidate* bestDirectional = nullptr;
                for (const NavigationCandidate& candidate : navigationCandidates) {
                    if (candidate.Item == focusedCandidate->Item) {
                        continue;
                    }

                    float score = 0.0f;
                    if (!ComputeDirectionalScore(directionVector, focusedCandidate->Center, candidate.Center, score)) {
                        continue;
                    }

                    const bool betterScore = score < bestScore;
                    const bool equalScoreHigherPriority = std::abs(score - bestScore) <= 0.001f
                        && bestDirectional
                        && IsHigherPriorityHit(candidate.Item, candidate.ZOrder, bestDirectional->Item, bestDirectional->ZOrder);
                    if (betterScore || equalScoreHigherPriority) {
                        bestScore = score;
                        bestDirectional = &candidate;
                    }
                }

                if (bestDirectional) {
                    nextFocus = bestDirectional->Item;
                } else if (world.Has<Components::GUINavigation>(focusedCandidate->Item)) {
                    const auto& nav = world.Get<Components::GUINavigation>(focusedCandidate->Item);
                    if (nav.Enabled && nav.Wrap) {
                        // Wrap mode picks the far-edge candidate opposite to travel direction.
                        float edgeValue = std::numeric_limits<float>::max();
                        float lateralValue = std::numeric_limits<float>::max();
                        const NavigationCandidate* wrapCandidate = nullptr;
                        const Vector2D perpendicular = { -directionVector.Y, directionVector.X };
                        for (const NavigationCandidate& candidate : navigationCandidates) {
                            const float projected = Dot2(candidate.Center, directionVector);
                            const float lateral = std::abs(Dot2(Sub2(candidate.Center, focusedCandidate->Center), perpendicular));
                            const bool betterProjected = projected < edgeValue - 0.001f;
                            const bool equalProjectedBetterLateral = std::abs(projected - edgeValue) <= 0.001f && lateral < lateralValue;
                            if (betterProjected || equalProjectedBetterLateral) {
                                edgeValue = projected;
                                lateralValue = lateral;
                                wrapCandidate = &candidate;
                            }
                        }

                        if (wrapCandidate) {
                            nextFocus = wrapCandidate->Item;
                        }
                    }
                }
            }

            if (!nextFocus.IsNull() && nextFocus != m_focusedEntity) {
                m_focusedEntity = nextFocus;
                if (m_activeGamepad >= 0 && Input::IsGamepadVibrationSupported(m_activeGamepad)) {
                    // Tiny pulse gives tactile confirmation without sustained rumble.
                    Input::SetGamepadVibration(m_activeGamepad, 0.08f, 0.12f);
                    Input::StopGamepadVibration(m_activeGamepad);
                }
            }
        }

        if (cancelTriggered && !m_captureEntity.IsNull()) {
            m_captureEntity = NULL_ENTITY;
            m_activeSlider = NULL_ENTITY;
            m_sliderAxisValid = false;
        }

        // Iterate GUI elements and update per-entity input state.
        world.Each<Components::GUIElement, Components::GUIInput>([&](Entity entity, Components::GUIElement& element, Components::GUIInput& input) {
            input.Clicked = false;
            input.Released = false;
            input.Entered = false;
            input.Exited = false;
            input.Focused = false;

            const bool interactable = IsEntityInteractable(world, entity, element);
            if (!interactable) {
                if (m_captureEntity == entity) {
                    m_captureEntity = NULL_ENTITY;
                }
                if (m_activeSlider == entity) {
                    m_activeSlider = NULL_ENTITY;
                    m_sliderAxisValid = false;
                }
                if (m_focusedEntity == entity) {
                    m_focusedEntity = NULL_ENTITY;
                }
                input.Hovered = false;
                input.Pressed = false;
                input.Dragging = false;
                if (world.Has<Components::GUISlider>(entity)) {
                    world.Get<Components::GUISlider>(entity).ValueChanged = false;
                }
                return;
            }

            const bool pointerHovered = (entity == topHovered);
            const bool focused = (entity == m_focusedEntity);
            const bool hovered = (m_inputMode == InputMode::Gamepad) ? focused : pointerHovered;
            const bool wasHovered = input.Hovered;
            input.Hovered = hovered;
            input.Entered = (!wasHovered && hovered);
            input.Exited = (wasHovered && !hovered);
            input.Focused = focused;

            if (mousePressed && pointerHovered) {
                m_captureEntity = entity;
                m_focusedEntity = entity;
            }

            const bool captured = (m_captureEntity == entity);
            input.Pressed = captured && mouseDown;
            input.Dragging = captured && mouseDown;

            if (captured && mouseReleased) {
                input.Pressed = false;
                input.Dragging = false;
                input.Released = true;
                if (pointerHovered) {
                    input.Clicked = true;
                }
                if (m_activeSlider == entity) {
                    m_activeSlider = NULL_ENTITY;
                    m_sliderAxisValid = false;
                }
                m_captureEntity = NULL_ENTITY;
            }

            if (captured && !mouseDown && !mousePressed) {
                input.Dragging = false;
                if (m_activeSlider == entity) {
                    m_activeSlider = NULL_ENTITY;
                    m_sliderAxisValid = false;
                }
                m_captureEntity = NULL_ENTITY;
            }

            if (focused && submitTriggered) {
                input.Clicked = true;
                input.Released = true;
                if (m_activeGamepad >= 0 && Input::IsGamepadVibrationSupported(m_activeGamepad)) {
                    Input::SetGamepadVibration(m_activeGamepad, 0.12f, 0.18f);
                    Input::StopGamepadVibration(m_activeGamepad);
                }
            }

            if (world.Has<Components::GUIButton>(entity)) {
                auto& button = world.Get<Components::GUIButton>(entity);
                if (input.Clicked && button.Toggle) {
                    button.Toggled = !button.Toggled;
                }
            }

            if (world.Has<Components::GUISlider>(entity)) {
                auto& slider = world.Get<Components::GUISlider>(entity);
                slider.ValueChanged = false;

                if (slider.Max <= slider.Min) {
                    if (m_activeSlider == entity) {
                        m_activeSlider = NULL_ENTITY;
                        m_sliderAxisValid = false;
                    }
                    return;
                }

                const bool isWorldSpace = (ResolveGUIRenderSpace(world, entity) == Components::GUIRenderSpace::World);
                SliderInteractionBasis basis;
                buildSliderBasis(entity, element, slider, isWorldSpace, basis);

                float cursorPrimaryCoord = 0.0f;
                float cursorSecondaryCoord = 0.0f;
                float tFromCursor = 0.0f;
                const bool basisProjected = ProjectCursorToSliderBasis(basis, mouse, cursorPrimaryCoord, cursorSecondaryCoord, tFromCursor);

                if (m_activeSlider == entity && !basisProjected) {
                    m_sliderAxisValid = false;
                }

                const bool startInteraction = mousePressed && captured;
                if (startInteraction && basisProjected) {
                    // Click-to-set uses absolute projection along the slider axis.
                    const float value = QuantizeAndClampSliderValue(
                        slider.Min + tFromCursor * (slider.Max - slider.Min), slider);
                    if (std::abs(value - slider.Value) > 0.0001f) {
                        slider.Value = value;
                        slider.ValueChanged = true;
                    }

                    m_activeSlider = entity;
                    m_sliderLastAxisCoord = cursorPrimaryCoord;
                    m_sliderAxisValid = true;
                }

                const bool dragInteraction = captured && mouseDown && !mousePressed && (m_activeSlider == entity);
                if (dragInteraction && basisProjected && m_sliderAxisValid && basis.PrimaryLength > 0.0001f) {
                    // Drag uses axis delta to preserve smooth movement across rotated sliders.
                    const float delta = cursorPrimaryCoord - m_sliderLastAxisCoord;
                    m_sliderLastAxisCoord = cursorPrimaryCoord;

                    const float value = QuantizeAndClampSliderValue(
                        slider.Value + (delta / basis.PrimaryLength) * (slider.Max - slider.Min), slider);
                    if (std::abs(value - slider.Value) > 0.0001f) {
                        slider.Value = value;
                        slider.ValueChanged = true;
                    }
                }

                if (focused && sliderAdjustedByController && sliderDirectionInt != 0) {
                    float step = slider.Step;
                    if (step <= 0.0f) {
                        step = std::max(0.0001f, (slider.Max - slider.Min) / 100.0f);
                    }

                    if (sliderCoarseStep || Input::IsGamepadButtonDown(m_activeGamepad, GAMEPAD_BUTTON_LEFT_BUMPER)
                        || Input::IsGamepadButtonDown(m_activeGamepad, GAMEPAD_BUTTON_RIGHT_BUMPER)) {
                        step *= 10.0f;
                    }

                    const float value = QuantizeAndClampSliderValue(
                        slider.Value + static_cast<float>(sliderDirectionInt) * step, slider);
                    if (std::abs(value - slider.Value) > 0.0001f) {
                        slider.Value = value;
                        slider.ValueChanged = true;
                        if (m_activeGamepad >= 0 && Input::IsGamepadVibrationSupported(m_activeGamepad)) {
                            Input::SetGamepadVibration(m_activeGamepad, 0.06f, 0.10f);
                            Input::StopGamepadVibration(m_activeGamepad);
                        }
                    }
                }
            }
        });
    }

    /**
     * @brief Tear down GUI input state before world shutdown.
     * @param world ECS world owning this system instance.
     * @return void
     * @complexity O(1).
     */
    void GUIInputSystem::OnDestroy(World& world) {
        (void)world;
        ResetState();
    }

    // Return metadata used for system registration.
    SystemMetadata GUIInputSystem::GetMetadata() const {
        ComponentAccessBuilder builder("GUIInputSystem");
        builder.SetExecutionOrder(-15);
        return builder
            .ReadComponent<Components::Active>()
            .ReadComponent<Components::Parent>()
            .ReadComponent<Components::GUIElement>()
            .ReadComponent<Components::GUINavigation>()
            .WriteComponent<Components::GUIInput>()
            .WriteComponent<Components::GUIButton>()
            .WriteComponent<Components::GUISlider>()
            .Build();
    }
}
