/* Start Header *****************************************************************/
/*!
\file   GUIInputSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Declaration of the GUIInputSystem class for handling GUI input processing.
Provides methods for system lifecycle management and metadata retrieval.
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/ISystem.h"

namespace ECS {
    // Handles input processing for GUI components.
    class GUIInputSystem : public ISystem {
    public:
        // Initialize system state when the world is created.
        void OnCreate(World& world) override;
        // Process GUI input for this frame.
        void OnUpdate(World& world) override;
        // Tear down any system state before shutdown.
        void OnDestroy(World& world) override;

        // Return system metadata for registration and inspection.
        SystemMetadata GetMetadata() const override;
        // Run alongside rendering systems.
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }
        // Always run to keep GUI input responsive.
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }

    private:
        // Reset transient input/capture state for this system instance.
        void ResetState();

        Entity m_captureEntity = NULL_ENTITY;
        Entity m_activeSlider = NULL_ENTITY;
        float m_sliderLastAxisCoord = 0.0f;
        bool m_sliderAxisValid = false;
    };
}
