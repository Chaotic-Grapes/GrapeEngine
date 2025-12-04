/* Start Header *****************************************************************/
/*!
\file   EditorIntegrationExample.h
\author GitHub Copilot
\brief
Example code demonstrating how to use the EditorCallbackRegistry and 
MessageSystem for Engine↔Editor communication.

This file serves as documentation and should not be compiled.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITORINTEGRATIONEXAMPLE_H
#define EDITORINTEGRATIONEXAMPLE_H

// This is an example/documentation file - not meant to be compiled

#if 0 // Documentation only

#include "core/EditorCallbacks.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

// ============================================================================
// EXAMPLE 1: Editor Registering Callbacks During Initialization
// ============================================================================

namespace Editor {

    class EditorApplication {
    public:
        void Initialize() {
            // Register callbacks with the engine
            auto& callbacks = Engine::EditorCallbackRegistry::Get();

            // Register entity selection handler
            callbacks.RegisterEntitySelection([this](uint32_t entityId, bool addToSelection) {
                this->hierarchyPanel->SelectEntity(entityId, addToSelection);
                this->inspectorPanel->InspectEntity(entityId);
            });

            // Register inspector refresh handler
            callbacks.RegisterInspectorRefresh([this](uint32_t entityId) {
                this->inspectorPanel->Refresh(entityId);
            });

            // Register notification handler
            callbacks.RegisterNotification([this](int severity, const std::string& title,
                                                 const std::string& message, float duration) {
                this->notificationSystem->ShowNotification(severity, title, message, duration);
            });

            // Register debug draw handler
            callbacks.RegisterDebugDraw([this](ECS::World* world) {
                this->DrawPhysicsColliders(world);
                this->DrawAudioSources(world);
                this->DrawSelectedEntityOutline(world);
            });

            // Register editor camera provider
            callbacks.RegisterEditorCamera([this](glm::vec3& pos, glm::mat4& view, glm::mat4& proj) {
                if (this->viewportPanel->HasEditorCamera()) {
                    auto* cam = this->viewportPanel->GetEditorCamera();
                    pos = cam->GetPosition();
                    view = cam->GetViewMatrix();
                    proj = cam->GetProjectionMatrix();
                    return true;
                }
                return false;
            });

            // Register viewport picking
            callbacks.RegisterPick([this](float screenX, float screenY) {
                return this->viewportPanel->PickEntityAtScreenPosition(screenX, screenY);
            });
        }

        void Shutdown() {
            // Clean up callbacks on shutdown
            Engine::EditorCallbackRegistry::Get().ClearAll();
        }

    private:
        HierarchyPanel* hierarchyPanel;
        InspectorPanel* inspectorPanel;
        ViewportPanel* viewportPanel;
        NotificationSystem* notificationSystem;

        void DrawPhysicsColliders(ECS::World* world);
        void DrawAudioSources(ECS::World* world);
        void DrawSelectedEntityOutline(ECS::World* world);
    };

} // namespace Editor

// ============================================================================
// EXAMPLE 2: Engine Invoking Callbacks (From Within Engine Systems)
// ============================================================================

namespace ECS {

    // Example: Physics system wants to notify editor about collision
    class PhysicsSystem {
    public:
        void OnCollisionDetected(uint32_t entityA, uint32_t entityB) {
            // Send message through message system
            Messaging::MessageSystem::Notify(
                Messaging::CollisionDetected{entityA, entityB, 10.0f}
            );

            // Optionally highlight the entities in editor
            if (Engine::EditorCallbackRegistry::Get().IsEditorActive()) {
                std::vector<uint32_t> entities = {entityA, entityB};
                Messaging::MessageSystem::Notify(
                    Messaging::EntityHighlightRequested{
                        entities,
                        glm::vec4(1.0f, 0.0f, 0.0f, 0.5f), // Red highlight
                        2.0f // Duration in seconds
                    }
                );
            }
        }
    };

    // Example: Renderer system providing debug visualization
    class RendererSystem {
    public:
        void RenderDebugInfo(ECS::World* world) {
            // Invoke editor's debug draw callback
            Engine::EditorCallbackRegistry::Get().InvokeDebugDraw(world);
        }

        void RenderSceneToTexture(uint32_t textureId, int width, int height) {
            // Notify editor that scene texture is ready
            Engine::EditorCallbackRegistry::Get().InvokeSceneRender(textureId, width, height);
        }
    };

    // Example: Entity manipulation with editor notification
    void DestroyEntity(ECS::World* world, uint32_t entityId) {
        // Perform the actual destruction
        world->Destroy(ECS::Entity{entityId});

        // Notify editor through message system
        Messaging::MessageSystem::Notify(
            Messaging::EntityDestroyed{entityId}
        );

        // Mark scene as modified
        Messaging::MessageSystem::Notify(
            Messaging::SceneModified{"Entity destroyed"}
        );

        // Clear selection if this was the selected entity
        if (Engine::EditorCallbackRegistry::Get().IsEditorActive()) {
            Engine::EditorCallbackRegistry::Get().InvokeEntitySelection(0); // 0 = clear
        }
    }

} // namespace ECS

// ============================================================================
// EXAMPLE 3: Editor Subscribing to Engine Events
// ============================================================================

namespace Editor {

    class HierarchyPanel {
    public:
        void Initialize() {
            // Subscribe to entity creation events
            m_entityCreatedHandle = Messaging::MessageSystem::Subscribe<Messaging::EntityCreated>(
                [this](const Messaging::EntityCreated& evt) {
                    this->AddEntityToHierarchy(evt.EntityId);
                }
            );

            // Subscribe to entity destruction events
            m_entityDestroyedHandle = Messaging::MessageSystem::Subscribe<Messaging::EntityDestroyed>(
                [this](const Messaging::EntityDestroyed& evt) {
                    this->RemoveEntityFromHierarchy(evt.EntityId);
                    if (m_selectedEntity == evt.EntityId) {
                        m_selectedEntity = 0; // Clear selection
                    }
                }
            );

            // Subscribe to scene modified events (for dirty flag)
            m_sceneModifiedHandle = Messaging::MessageSystem::Subscribe<Messaging::SceneModified>(
                [this](const Messaging::SceneModified& evt) {
                    this->MarkSceneDirty();
                }
            );
        }

        void Shutdown() {
            // Unsubscribe from all events
            Messaging::MessageSystem::Unsubscribe<Messaging::EntityCreated>(m_entityCreatedHandle);
            Messaging::MessageSystem::Unsubscribe<Messaging::EntityDestroyed>(m_entityDestroyedHandle);
            Messaging::MessageSystem::Unsubscribe<Messaging::SceneModified>(m_sceneModifiedHandle);
        }

    private:
        Messaging::SubscriptionHandle m_entityCreatedHandle;
        Messaging::SubscriptionHandle m_entityDestroyedHandle;
        Messaging::SubscriptionHandle m_sceneModifiedHandle;
        uint32_t m_selectedEntity = 0;

        void AddEntityToHierarchy(uint32_t entityId);
        void RemoveEntityFromHierarchy(uint32_t entityId);
        void MarkSceneDirty();
    };

    class InspectorPanel {
    public:
        void Initialize() {
            // Subscribe to transform changes (for undo system)
            m_transformChangedHandle = Messaging::MessageSystem::Subscribe<Messaging::EntityTransformChanged>(
                [this](const Messaging::EntityTransformChanged& evt) {
                    // Only care about the currently inspected entity
                    if (evt.EntityId == m_inspectedEntity) {
                        this->RefreshTransformUI();
                    }
                    // Record in undo system
                    this->undoSystem->RecordTransformChange(evt);
                },
                10 // High priority to capture before other systems
            );

            // Subscribe to editor notifications
            m_notificationHandle = Messaging::MessageSystem::Subscribe<Messaging::EditorNotificationRequested>(
                [this](const Messaging::EditorNotificationRequested& evt) {
                    this->ShowNotification(evt.Level, evt.Title, evt.Message, evt.Duration);
                }
            );
        }

        void Shutdown() {
            Messaging::MessageSystem::Unsubscribe<Messaging::EntityTransformChanged>(m_transformChangedHandle);
            Messaging::MessageSystem::Unsubscribe<Messaging::EditorNotificationRequested>(m_notificationHandle);
        }

    private:
        Messaging::SubscriptionHandle m_transformChangedHandle;
        Messaging::SubscriptionHandle m_notificationHandle;
        uint32_t m_inspectedEntity = 0;

        void RefreshTransformUI();
        void ShowNotification(int severity, const std::string& title, 
                            const std::string& message, float duration);
    };

} // namespace Editor

// ============================================================================
// EXAMPLE 4: Bidirectional Communication Pattern
// ============================================================================

namespace Editor {

    class ViewportPanel {
    public:
        void OnMouseClick(float screenX, float screenY) {
            // Editor requests engine to perform picking
            Messaging::PickResultRequested pickRequest(screenX, screenY);
            
            // Option A: Direct callback (synchronous)
            uint32_t pickedEntity = Engine::EditorCallbackRegistry::Get().InvokePick(screenX, screenY);
            if (pickedEntity != 0) {
                // Select the picked entity
                Engine::EditorCallbackRegistry::Get().InvokeEntitySelection(pickedEntity);
            }

            // Option B: Message-based (can be asynchronous)
            // Send pick request message
            Messaging::MessageSystem::Notify(pickRequest);
            // Engine fills in the result
            // Editor reads result from the message
        }

        void OnEntityDraggedInScene(uint32_t entityId, const glm::vec3& newPosition) {
            // Update entity through engine
            // (actual entity manipulation code here)
            
            // Notify that scene was modified
            Messaging::MessageSystem::Notify(
                Messaging::SceneModified{"Entity moved in viewport"}
            );
            
            // Refresh inspector to show new position
            Engine::EditorCallbackRegistry::Get().InvokeInspectorRefresh(entityId);
        }
    };

} // namespace Editor

// ============================================================================
// EXAMPLE 5: System-Specific Debug Visualization
// ============================================================================

namespace ECS {

    class PhysicsSystem {
    public:
        void DrawDebugVisualization(ECS::World* world) {
            // Physics system only draws debug info when editor is active
            if (!Engine::EditorCallbackRegistry::Get().IsEditorActive()) {
                return; // Don't waste time in game builds
            }

            // Send debug visualization request
            Messaging::MessageSystem::Notify(
                Messaging::DebugVisualizationRequested{
                    Messaging::DebugVisualizationRequested::Type::Physics,
                    true,
                    GetDebugData() // Pass physics debug data
                }
            );
        }

    private:
        void* GetDebugData() {
            // Return pointer to physics debug data structure
            return nullptr;
        }
    };

} // namespace ECS

// ============================================================================
// EXAMPLE 6: Error Handling and Notifications
// ============================================================================

namespace Engine {

    class ResourceManager {
    public:
        bool LoadTexture(const std::string& path) {
            // Attempt to load texture
            if (!DoActualLoad(path)) {
                // Notify editor of failure
                if (EditorCallbackRegistry::Get().IsEditorActive()) {
                    EditorCallbackRegistry::Get().InvokeNotification(
                        2, // Error severity
                        "Resource Load Failed",
                        "Failed to load texture: " + path,
                        5.0f // Show for 5 seconds
                    );
                }

                // Also send through message system for logging
                Messaging::MessageSystem::Notify(
                    Messaging::ResourceLoaded{path, "Texture", false}
                );

                return false;
            }

            // Success notification
            Messaging::MessageSystem::Notify(
                Messaging::ResourceLoaded{path, "Texture", true}
            );

            return true;
        }

    private:
        bool DoActualLoad(const std::string& path);
    };

} // namespace Engine

#endif // Documentation only

#endif // EDITORINTEGRATIONEXAMPLE_H
