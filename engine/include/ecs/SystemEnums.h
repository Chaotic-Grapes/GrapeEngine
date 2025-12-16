/* Start Header *****************************************************************/
/*!
\file    SystemEnums.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Enumeration definitions for ECS system execution control.

This header contains enum definitions that are needed by multiple ECS headers
to avoid circular dependencies.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SYSTEM_ENUMS_H
#define SYSTEM_ENUMS_H

namespace ECS {

    /**
     * @brief System run mode for editor play/edit state control
     */
    enum class SystemRunMode {
        Always,         // Runs in both edit and play mode (e.g., Render, Transform hierarchy)
        PlayOnly,       // Only runs in play mode (e.g., Physics, Scripts, AI, Animation)
        EditOnly        // Only runs in edit mode (e.g., Editor gizmos, debug visualization)
    };

    /**
     * @brief System execution group for phased updates
     */
    enum class SystemGroup {
        PreUpdate,      // Input handling, pre-simulation setup
        Update,         // Main gameplay logic (C# systems mostly here)
        PostUpdate,     // Post-gameplay processing
        PrePhysics,     // Physics preparation
        Physics,        // Physics simulation
        PostPhysics,    // Physics response, collision callbacks
        PreRender,      // Frustum culling, LOD selection
        Render,         // Actual rendering
        PostRender      // Cleanup, debug visualization
    };

}

#endif
