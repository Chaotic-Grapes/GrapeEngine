/* Start Header *****************************************************************/
/*!
\file   PhysicsDebugDraw2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Debug draw callback interface for the 2D physics runtime.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_PHYSICSDEBUGDRAW2D_H
#define ENGINE_PHYSICS2D_PHYSICSDEBUGDRAW2D_H

#include "math/Vector2D.h"

namespace Engine::Physics2D {
    class IPhysicsDebugDraw2D {
    public:
        /**
         * @brief Virtual destructor for interface safety.
         */
        virtual ~IPhysicsDebugDraw2D() = default;

        /**
         * @brief Draw an axis-aligned bounding box.
         * @param min Minimum world corner.
         * @param max Maximum world corner.
         */
        virtual void DrawAABB(const Vector2D& min, const Vector2D& max) = 0;

        /**
         * @brief Draw a contact point and its world normal.
         * @param point World-space contact point.
         * @param normal World-space contact normal.
         */
        virtual void DrawContactPoint(const Vector2D& point, const Vector2D& normal) = 0;
    };
}

#endif
