#include "ecs/systems/UIEventSystem.h"
#include "ecs/Components.h"
#include "services/Input.h"
#include "core/Logger.h"

namespace ECS {

    // Static member initialization
    Entity UIEventSystem::s_hoveredEntity = NULL_ENTITY;
    Entity UIEventSystem::s_previousHoveredEntity = NULL_ENTITY;

    void UIEventSystem::Initialize() {
        s_hoveredEntity = NULL_ENTITY;
        s_previousHoveredEntity = NULL_ENTITY;
        LOG_DEBUG("[UIEventSystem] Initialized");
    }

    void UIEventSystem::Update(World* world, uint32_t pickedEntityID, const Vector2D& mouseScreenPos) {
        // Convert picked ID to entity (picking system uses Index + 1)
        Entity currentEntity = NULL_ENTITY;
        if (pickedEntityID > 0 && pickedEntityID != 0xFFFFFFFF) {
            uint32_t entityIndex = pickedEntityID - 1;

            // Optionally validate with world if provided
            if (world) {
                currentEntity = world->Resolve(entityIndex);
                if (!world->IsAlive(currentEntity)) {
                    currentEntity = NULL_ENTITY;
                }
            }
            else {
                // No world validation - just trust the picked ID
                currentEntity = Entity{ entityIndex, 0 };
            }
        }

        // Handle hover state changes
        if (currentEntity != s_previousHoveredEntity) {
            // Exit previous hovered entity
            if (!s_previousHoveredEntity.IsNull()) {
                UIEvent event;
                event.SelfEntity = s_previousHoveredEntity;
                event.Type = UIEventType::HoverExit;
                event.ScreenPosition = mouseScreenPos;
                event.Button = -1;
                UIEventQueue::AddEvent(event);
            }

            // Enter new hovered entity
            if (!currentEntity.IsNull()) {
                UIEvent event;
                event.SelfEntity = currentEntity;
                event.Type = UIEventType::HoverEnter;
                event.ScreenPosition = mouseScreenPos;
                event.Button = -1;
                UIEventQueue::AddEvent(event);
            }
        }

        // Handle click on currently hovered entity
        if (!currentEntity.IsNull() && Input::IsMousePressed(MOUSE_LEFT)) {
            UIEvent event;
            event.SelfEntity = currentEntity;
            event.Type = UIEventType::Click;
            event.ScreenPosition = mouseScreenPos;
            event.Button = MOUSE_LEFT;
            UIEventQueue::AddEvent(event);

            LOG_INFO("[UIEventSystem] Entity clicked! ID: " << currentEntity.Index);
        }

        // Update state for next frame
        s_previousHoveredEntity = s_hoveredEntity;
        s_hoveredEntity = currentEntity;
    }

} // namespace ECS