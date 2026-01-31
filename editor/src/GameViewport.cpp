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

// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------
void GameViewport::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    if (m_undoSystem) {
        m_undoSystem->Update();
    }

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
void GameViewport::_renderViewport() {
    // Begin game viewport window
    ImGui::Begin("Game", nullptr);

    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem) {
        ImGui::TextDisabled("No game renderer not initialized");
    }
    else {
        const ECS::ComponentTypeId cameraId = Editor::ECSUtils::GetComponentIdFromName("Camera3D");
        // Check if there's an active camera in the scene
        bool hasCameraComponent = false;
        bool hasActiveCamera = false;
        int cameraCount = 0;

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
                }
            });
        }

        if (!hasCameraComponent) {
            ImGui::TextDisabled("No camera found");
            ImGui::TextDisabled("Add a Camera3D component to an entity");
        }
        else if (!hasActiveCamera) {
            ImGui::Text("Found %d camera(s) but none are active", cameraCount);
            ImGui::TextDisabled("Set Camera3D.Active to true in the inspector");
        }
        else {
            // Aspect ratio selector panel
            {
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
                
                ImGui::PopStyleVar(2);
                ImGui::Separator();
            }

            auto availableSize = ImGui::GetContentRegionAvail();
            
            // Calculate display size based on aspect ratio
            ImVec2 displaySize = availableSize;
            float targetRatio = availableSize.x / availableSize.y;
            
            if (!m_freeAspect) {
                switch (m_selectedAspectRatio) {
                    case 1: targetRatio = 16.0f / 9.0f; break;   // 16:9
                    case 2: targetRatio = 16.0f / 10.0f; break;  // 16:10
                    case 3: targetRatio = 4.0f / 3.0f; break;    // 4:3
                    case 4: targetRatio = 5.0f / 4.0f; break;    // 5:4
                    case 5: targetRatio = 21.0f / 9.0f; break;   // 21:9
                    case 6: targetRatio = 1.0f; break;           // 1:1
                }
                
                const float availableRatio = availableSize.x / availableSize.y;
                
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
            
            // Update camera aspect ratio to match display size
            if (m_world && cameraId != ECS::NULL_COMPONENT_ID) {
                m_world->Each([&](ECS::Entity e) {
                    if (!m_world->HasById(e, cameraId)) {
                        return;
                    }

                    auto* cam = static_cast<ECS::Components::Camera3D*>(m_world->GetRawComponentPtr(e, cameraId));
                    if (cam && cam->Active) {
                        cam->AspectRatio = targetRatio;
                    }
                });
            }

            // NOTE: The Game viewport displays the rendered scene output.
            // The renderer is configured by LevelEditor::Update() to use the editor camera.
            // To view the game camera perspective, the Editor Camera would need to be
            // positioned at the game camera's location, or we would need separate render passes.

            auto* rg = rendererSystem->GetRenderGraph();
            if (rg) {
                ResourceAccessor acc(rg);
                const uint32_t textureId = acc.GetTexture("LDR");
                if (textureId > 0) {
                    ImGui::Image(textureId, displaySize, ImVec2(0, 1), ImVec2(1, 0));
                }
                else {
                    ImGui::TextDisabled("Texture ID is 0 - render graph issue");
                }
            }
            else {
                ImGui::TextDisabled("No render graph available");
            }
        }
    }

    ImGui::End();
}
