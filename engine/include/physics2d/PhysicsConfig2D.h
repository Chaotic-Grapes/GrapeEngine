/* Start Header *****************************************************************/
/*!
\file   PhysicsConfig2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief
Configuration and tolerance values for the fixed-step 2D physics runtime.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_PHYSICSCONFIG2D_H
#define ENGINE_PHYSICS2D_PHYSICSCONFIG2D_H

#include <cstdint>

namespace Engine::Physics2D {
    /**
     * @brief Runtime tuning parameters for fixed-step 2D simulation.
     */
    struct PhysicsConfig2D {
        float FixedDeltaSeconds = 1.0f / 60.0f;
        uint32_t MaxFixedStepsPerFrame = 5;
        uint32_t VelocityIterations = 8;
        uint32_t PositionIterations = 3;
        float SpeculativeCCDVelocityThreshold = 12.0f;
        float SleepLinearThreshold = 0.03f;
        float SleepAngularThreshold = 0.03f;
        float SleepTimeThresholdSeconds = 0.6f;
        float ContactSlop = 0.0001f;
        float PositionCorrectionPercent = 0.4f;
        float FatAABBPadding = 0.2f;
    };
}

#endif
