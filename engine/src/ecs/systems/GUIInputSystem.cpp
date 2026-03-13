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
#include <glm/glm.hpp>
#include "ecs/Components.h"
#include "ecs/systems/GUIInputSystem.h"
#include "ecs/systems/GUIRenderUtils.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/renderer.hpp"
#include "services/Input.h"

namespace ECS {
    namespace {
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

    void GUIInputSystem::ResetState() {
        m_captureEntity = NULL_ENTITY;
        m_activeSlider = NULL_ENTITY;
        m_sliderLastAxisCoord = 0.0f;
        m_sliderAxisValid = false;
    }

    // Initialize GUI input system state for the world.
    void GUIInputSystem::OnCreate(World& world) {
        (void)world;
        ResetState();
    }

    // Update GUI hover/press state from the current input.
    void GUIInputSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        if (!m_captureEntity.IsNull() && !world.IsAlive(m_captureEntity)) {
            m_captureEntity = NULL_ENTITY;
        }
        if (!m_activeSlider.IsNull() && !world.IsAlive(m_activeSlider)) {
            m_activeSlider = NULL_ENTITY;
            m_sliderAxisValid = false;
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

        // Snapshot mouse button state for this frame.
        const bool mouseDown = Input::IsMouseDown(MOUSE_LEFT);
        const bool mousePressed = Input::IsMousePressed(MOUSE_LEFT);
        const bool mouseReleased = Input::IsMouseUp(MOUSE_LEFT);

        // Resolve top-most hovered element by z-order and stable entity tie-break.
        Entity topHovered = NULL_ENTITY;
        int16_t topHoveredZ = std::numeric_limits<int16_t>::min();
        world.Each<Components::GUIElement, Components::GUIInput>([&](Entity entity, Components::GUIElement& element, Components::GUIInput&) {
            if (!IsEntityInteractable(world, entity, element)) {
                return;
            }

            const bool isWorldSpace = (ResolveGUIRenderSpace(world, entity) == Components::GUIRenderSpace::World);
            bool hit = false;

            if (world.Has<Components::GUISlider>(entity)) {
                const auto& slider = world.Get<Components::GUISlider>(entity);
                SliderInteractionBasis basis;
                buildSliderBasis(entity, element, slider, isWorldSpace, basis);

                float axisCoord = 0.0f;
                float tFromCursor = 0.0f;
                hit = IsCursorInsideSliderTrack(basis, mouse, axisCoord, tFromCursor);
            } else {
                const Vector2D pos = isWorldSpace ? element.ScreenPosition : element.ResolvedPosition;
                const Vector2D size = isWorldSpace ? element.ScreenSize : element.ResolvedSize;
                hit = PointInRect(mouse, pos, size);
            }

            if (!hit) {
                return;
            }

            if (topHovered.IsNull() || IsHigherPriorityHit(entity, element.ZOrder, topHovered, topHoveredZ)) {
                topHovered = entity;
                topHoveredZ = element.ZOrder;
            }
        });

        // Iterate GUI elements and update per-entity input state.
        world.Each<Components::GUIElement, Components::GUIInput>([&](Entity entity, Components::GUIElement& element, Components::GUIInput& input) {
            input.Clicked = false;
            input.Released = false;
            input.Entered = false;
            input.Exited = false;

            const bool interactable = IsEntityInteractable(world, entity, element);
            if (!interactable) {
                if (m_captureEntity == entity) {
                    m_captureEntity = NULL_ENTITY;
                }
                if (m_activeSlider == entity) {
                    m_activeSlider = NULL_ENTITY;
                    m_sliderAxisValid = false;
                }
                input.Hovered = false;
                input.Pressed = false;
                input.Dragging = false;
                if (world.Has<Components::GUISlider>(entity)) {
                    world.Get<Components::GUISlider>(entity).ValueChanged = false;
                }
                return;
            }

            const bool hovered = (entity == topHovered);
            const bool wasHovered = input.Hovered;
            input.Hovered = hovered;
            input.Entered = (!wasHovered && hovered);
            input.Exited = (wasHovered && !hovered);

            if (mousePressed && hovered) {
                m_captureEntity = entity;
            }

            const bool captured = (m_captureEntity == entity);
            input.Pressed = captured && mouseDown;
            input.Dragging = captured && mouseDown;

            if (captured && mouseReleased) {
                input.Pressed = false;
                input.Dragging = false;
                input.Released = true;
                if (hovered) {
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
                    const float delta = cursorPrimaryCoord - m_sliderLastAxisCoord;
                    m_sliderLastAxisCoord = cursorPrimaryCoord;

                    const float value = QuantizeAndClampSliderValue(
                        slider.Value + (delta / basis.PrimaryLength) * (slider.Max - slider.Min), slider);
                    if (std::abs(value - slider.Value) > 0.0001f) {
                        slider.Value = value;
                        slider.ValueChanged = true;
                    }
                }
            }
        });
    }

    // Tear down GUI input system state.
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
            .WriteComponent<Components::GUIInput>()
            .WriteComponent<Components::GUIButton>()
            .WriteComponent<Components::GUISlider>()
            .Build();
    }
}
