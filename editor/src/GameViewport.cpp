/* Start Header *****************************************************************/
/*!
\file   GameViewport.cpp
\author Samantha Leong (50%)
        Foo Rui Qin    (50%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implementation of GameViewport class for the game view with ECS scene camera and
aspect ratio selection. Separate from the editor camera and scene editing controls.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include "GameViewport.h"
#include <imgui.h>
#include "ecs/systems/RendererSystem.h"  
#include "graphics/RenderGraph.hpp"
#include "core/Application.h"
#include "ecs/Components.h"
#include "EditorECSUtils.h"
#include "graphics/Viewport.hpp"
#include "helpers/TransformUtils.h"
#include "services/Input.h"
#include <algorithm>

namespace {
    constexpr const char* kGameViewportName = "Game";

    float ResolveSelectedAspectRatio(int selectedAspectRatio, bool freeAspect, int width, int height) {
        if (freeAspect) {
            return (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        }

        switch (selectedAspectRatio) {
        case 1: return 16.0f / 9.0f;
        case 2: return 16.0f / 10.0f;
        case 3: return 4.0f / 3.0f;
        case 4: return 5.0f / 4.0f;
        case 5: return 21.0f / 9.0f;
        case 6: return 1.0f;
        default: return 16.0f / 9.0f;
        }
    }
}

void GameViewport::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
    ECS::World* world, Scenes::SceneManager* sceneManager) {
    BaseViewport::Initialize(mainFont, boldFont, symbolsFont, world, sceneManager);
    SetViewportType(ViewportType::Game);
    SetViewportName(kGameViewportName);
    Graphics::ViewportManager::Create(kGameViewportName, nullptr, 1, 1);
}

GameViewport::~GameViewport() {
    Graphics::ViewportManager::Destroy(kGameViewportName);
}

// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------
void GameViewport::HandleInWorldInteraction() {
    if (Input::IsKeyPressed(KEY_F12)) {
        if (m_immersiveMode) {
            m_immersiveMode = false;
            m_requestRestore = true;
        } else {
            m_immersiveMode = true;
            m_requestRestore = false;
        }
    }
    if (m_immersiveMode && Input::IsKeyPressed(KEY_ESCAPE)) {
        m_immersiveMode = false;
        m_requestRestore = true;
    }

    if (!HasValidWorld()) return;

    // Game viewport doesn't handle entity dragging or editor camera input
    m_isViewportHovered = false;
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------
void GameViewport::ShowEditorWindows() {
    _renderViewport();
}

// -------------------------------------------------------------------------
// Private Rendering Implementation
// -------------------------------------------------------------------------
void GameViewport::PrepareFrame() {
    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem) {
        return;
    }

    ECS::Components::Camera3D* activeCamera = nullptr;
    ECS::Entity activeCameraEntity = ECS::NULL_ENTITY;
    const ECS::ComponentTypeId cameraId = Editor::ECSUtils::GetComponentIdFromName("Camera3D");

    if (m_world && cameraId != ECS::NULL_COMPONENT_ID) {
        m_world->Each([&](ECS::Entity e) {
            if (!m_world->HasById(e, cameraId)) {
                return;
            }

            auto* cam = static_cast<ECS::Components::Camera3D*>(m_world->GetRawComponentPtr(e, cameraId));
            if (!cam) {
                return;
            }

            if (cam->Active && !activeCamera) {
                activeCamera = cam;
                activeCameraEntity = e;
            }
        });
    }

    auto* vp = rendererSystem->GetViewport(kGameViewportName);
    if (!vp) {
        Graphics::ViewportManager::Create(kGameViewportName, nullptr, 1, 1);
        vp = rendererSystem->GetViewport(kGameViewportName);
    }
    if (!vp) {
        return;
    }

    const float targetRatio = ResolveSelectedAspectRatio(
        m_selectedAspectRatio,
        m_freeAspect,
        vp->Size.x,
        vp->Size.y
    );

    if (activeCamera) {
        activeCamera->AspectRatio = targetRatio;
    }

    const bool synced = (activeCamera != nullptr) &&
        _syncGameCamera(activeCameraEntity, *activeCamera, targetRatio);
    Graphics::ViewportManager::SetCamera(kGameViewportName, synced ? &m_gameCamera : nullptr);

    // Keep GUI layout/input in the same coordinate space as the visible Game viewport image.
    // This aligns editor Game view behavior with standalone runtime.
    ImVec2 guiOrigin = m_sceneDrawPos;
    ImVec2 guiSize = m_sceneDrawSize;
    if (guiSize.x <= 1.0f || guiSize.y <= 1.0f) {
        guiOrigin = ImVec2(0.0f, 0.0f);
        guiSize = ImVec2(static_cast<float>(vp->Size.x), static_cast<float>(vp->Size.y));
    }

    const ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
    rendererSystem->SetGUIViewport(
        Vector2D{ guiOrigin.x, guiOrigin.y },
        Vector2D{ guiSize.x, guiSize.y },
        Vector2D{
            std::max(0.0001f, fbScale.x),
            std::max(0.0001f, fbScale.y)
        }
    );
}

bool GameViewport::_syncGameCamera(ECS::Entity entity, const ECS::Components::Camera3D& camera, float targetAspect) {
    if (!m_world || entity.IsNull()) {
        return false;
    }

    m_gameCamera.UsePerspective = camera.UsePerspective;
    m_gameCamera.FOV = camera.FOV;
    m_gameCamera.NearPlane = camera.NearPlane;
    m_gameCamera.FarPlane = camera.FarPlane;
    m_gameCamera.OrthoSize = camera.OrthoSize;
    m_gameCamera.AspectRatio = targetAspect;

    ECS::Components::WorldTransform* worldTransform = nullptr;
    ECS::Components::LocalTransform* localTransform = nullptr;
    const ECS::ComponentTypeId worldId = Editor::ECSUtils::GetComponentIdFromName("WorldTransform");
    const ECS::ComponentTypeId localId = Editor::ECSUtils::GetComponentIdFromName("LocalTransform");
    if (worldId != ECS::NULL_COMPONENT_ID && m_world->HasById(entity, worldId)) {
        worldTransform = static_cast<ECS::Components::WorldTransform*>(
            m_world->GetRawComponentPtr(entity, worldId));
    }
    if (localId != ECS::NULL_COMPONENT_ID && m_world->HasById(entity, localId)) {
        localTransform = static_cast<ECS::Components::LocalTransform*>(
            m_world->GetRawComponentPtr(entity, localId));
    }

    bool useWorld = (worldTransform != nullptr);
    if (worldTransform && localTransform) {
        const float localLenSq = localTransform->Position.X * localTransform->Position.X +
            localTransform->Position.Y * localTransform->Position.Y +
            localTransform->Position.Z * localTransform->Position.Z;
        const bool worldAtOrigin = std::abs(worldTransform->Matrix.m03) < 1e-4f &&
            std::abs(worldTransform->Matrix.m13) < 1e-4f &&
            std::abs(worldTransform->Matrix.m23) < 1e-4f;
        if (worldAtOrigin && localLenSq > 1e-6f) {
            useWorld = false;
        }
    }

    if (useWorld && worldTransform) {
        Vector3D position;
        Vector3D scale;
        Quaternion rotation;
        TransformUtils::DecomposeTRS(worldTransform->Matrix, position, rotation, scale);
        m_gameCamera.Position = glm::vec3(position.X, position.Y, position.Z);
        m_gameCamera.Rotation = glm::quat(rotation.W, rotation.X, rotation.Y, rotation.Z);
        return true;
    }

    if (localTransform) {
        m_gameCamera.Position = glm::vec3(localTransform->Position.X, localTransform->Position.Y, localTransform->Position.Z);
        m_gameCamera.Rotation = glm::quat(localTransform->Rotation.W, localTransform->Rotation.X, localTransform->Rotation.Y, localTransform->Rotation.Z);
        return true;
    }

    return false;
}

void GameViewport::_renderViewport() {
    ImGuiWindowFlags windowFlags = 0;
    const bool renderImmersive = m_immersiveMode;
    if (!renderImmersive && m_requestRestore && m_restoreDockValid) {
        if (m_restoreDockId != 0) {
            ImGui::SetNextWindowDockID(m_restoreDockId, ImGuiCond_Always);
        } else {
            ImGui::SetNextWindowPos(m_restorePos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(m_restoreSize, ImGuiCond_Always);
        }
        m_requestRestore = false;
    } else if (renderImmersive) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
        windowFlags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                       ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    }

    // Begin game viewport window
    ImGui::Begin("Game", nullptr, windowFlags);
    if (!renderImmersive) {
        m_restoreDockId = ImGui::GetWindowDockID();
        m_restoreDockValid = true;
        m_restorePos = ImGui::GetWindowPos();
        m_restoreSize = ImGui::GetWindowSize();
    }

    bool hasCameraComponent = false;
    bool hasActiveCamera = false;
    int cameraCount = 0;
    ECS::Entity activeCameraEntity = ECS::NULL_ENTITY;
    ECS::Components::Camera3D* activeCamera = nullptr;
    uint32_t textureId = 0;

    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem) {
        ImGui::TextDisabled("No game renderer not initialized");
        m_isViewportHovered = false;
    }
    else {
        const ECS::ComponentTypeId cameraId = Editor::ECSUtils::GetComponentIdFromName("Camera3D");

        if (m_world && cameraId != ECS::NULL_COMPONENT_ID) {
            m_world->Each([&](ECS::Entity e) {
                if (!m_world->HasById(e, cameraId)) {
                    return;
                }

                auto* cam = static_cast<ECS::Components::Camera3D*>(m_world->GetRawComponentPtr(e, cameraId));
                if (!cam) {
                    return;
                }

                hasCameraComponent = true;
                cameraCount++;
                if (cam->Active) {
                    hasActiveCamera = true;
                    if (!activeCamera) {
                        activeCamera = cam;
                        activeCameraEntity = e;
                    }
                }
            });
        }

        if (!hasCameraComponent) {
            ImGui::TextDisabled("No camera found");
            ImGui::TextDisabled("Add a Camera3D component to an entity");
            Graphics::ViewportManager::SetCamera(kGameViewportName, nullptr);
            m_isViewportHovered = false;
        }
        else if (!hasActiveCamera) {
            ImGui::Text("Found %d camera(s) but none are active", cameraCount);
            ImGui::TextDisabled("Set Camera3D.Active to true in the inspector");
            Graphics::ViewportManager::SetCamera(kGameViewportName, nullptr);
            m_isViewportHovered = false;
        }
        else {
            if (!renderImmersive) {
                // Aspect ratio selector panel
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

                const char* aspectRatios[] = {
                    "Free Aspect",
                    "16:9",
                    "16:10",
                    "4:3",
                    "5:4",
                    "21:9",
                    "1:1"
                };
                
                ImGui::SetNextItemWidth(150);
                if (ImGui::Combo("##AspectRatio", &m_selectedAspectRatio, aspectRatios, IM_ARRAYSIZE(aspectRatios))) {
                    m_freeAspect = (m_selectedAspectRatio == 0);
                }

                ImGui::SameLine();
                if (ImGui::Button("Fullscreen (F12)")) {
                    m_immersiveMode = true;
                    m_requestRestore = false;
                }

                ImGui::PopStyleVar(2);
                ImGui::Separator();
            }

            auto availableSize = ImGui::GetContentRegionAvail();
            
            // Calculate display size based on aspect ratio
            ImVec2 displaySize = availableSize;
            const float availableHeight = std::max(1.0f, availableSize.y);
            const float availableRatio = availableSize.x / availableHeight;
            const float targetRatio = ResolveSelectedAspectRatio(
                m_selectedAspectRatio,
                m_freeAspect,
                static_cast<int>(availableSize.x),
                static_cast<int>(availableHeight)
            );

            if (!m_freeAspect) {
                if (availableRatio > targetRatio) {
                    // Available space is wider - constrain width
                    displaySize.x = availableSize.y * targetRatio;
                    displaySize.y = availableSize.y;
                }
                else {
                    // Available space is taller - constrain height
                    displaySize.x = availableSize.x;
                    displaySize.y = availableSize.x / targetRatio;
                }

                // Center the viewport
                const float offsetX = (availableSize.x - displaySize.x) * 0.5f;
                const float offsetY = (availableSize.y - displaySize.y) * 0.5f;
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));
            }

            // NOTE: The Game viewport displays the output of its dedicated renderer viewport.

            if (auto* vp = rendererSystem->GetViewport(kGameViewportName)) {
                if (vp->LDR) {
                    textureId = vp->LDR->GetColorTexture(0);
                }
            } else if (auto* rg = rendererSystem->GetRenderGraph()) {
                ResourceAccessor acc(rg);
                textureId = acc.GetTexture("LDR");
            }

            if (textureId > 0) {
                ImGui::Image(textureId, displaySize, ImVec2(0, 1), ImVec2(1, 0));
                const bool isGameImageHovered = ImGui::IsItemHovered();
                m_isViewportHovered = isGameImageHovered;
                m_sceneDrawPos = ImGui::GetItemRectMin();
                m_sceneDrawSize = displaySize;
            }
            else {
                ImGui::TextDisabled("Viewport texture unavailable");
                m_isViewportHovered = false;
            }

            if (displaySize.x > 1.0f && displaySize.y > 1.0f) {
                if (!rendererSystem->GetViewport(kGameViewportName)) {
                    Graphics::ViewportManager::Create(kGameViewportName, nullptr,
                        static_cast<int>(displaySize.x), static_cast<int>(displaySize.y));
                }
                Graphics::ViewportManager::Resize(kGameViewportName,
                    static_cast<int>(displaySize.x), static_cast<int>(displaySize.y));
            }
        }
    }

    ImGui::End();
    if (renderImmersive) {
        ImGui::PopStyleVar();
    }
}
