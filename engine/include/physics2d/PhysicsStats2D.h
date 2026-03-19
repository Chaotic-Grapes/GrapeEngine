/* Start Header *****************************************************************/
/*!
\file   PhysicsStats2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Lightweight profiling counters for PhysicsWorld2D stages.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_PHYSICSSTATS2D_H
#define ENGINE_PHYSICS2D_PHYSICSSTATS2D_H

#include <cstdint>

namespace Engine::Physics2D {
    /**
     * @brief Per-frame counters and timing values for physics stages.
     */
    struct PhysicsStats2D {
        uint32_t BodyCount = 0;
        uint32_t ActiveBodyCount = 0;
        uint32_t SleepingBodyCount = 0;
        uint32_t PairCount = 0;
        uint32_t ManifoldCount = 0;
        uint32_t ConstraintCount = 0;
        uint32_t BroadphaseTaskCount = 0;
        uint32_t NarrowphaseTaskCount = 0;
        uint32_t SolverTaskCount = 0;

        float SyncMs = 0.0f;
        float BroadphaseMs = 0.0f;
        float NarrowphaseMs = 0.0f;
        float SolveMs = 0.0f;
        float SleepMs = 0.0f;
        float EventsMs = 0.0f;
        float WritebackMs = 0.0f;

        /**
         * @brief Reset per-frame counters and timing accumulators.
         */
        void ResetFrame() {
            BodyCount = 0;
            ActiveBodyCount = 0;
            SleepingBodyCount = 0;
            PairCount = 0;
            ManifoldCount = 0;
            ConstraintCount = 0;
            BroadphaseTaskCount = 0;
            NarrowphaseTaskCount = 0;
            SolverTaskCount = 0;
            SyncMs = 0.0f;
            BroadphaseMs = 0.0f;
            NarrowphaseMs = 0.0f;
            SolveMs = 0.0f;
            SleepMs = 0.0f;
            EventsMs = 0.0f;
            WritebackMs = 0.0f;
        }
    };
}

#endif
