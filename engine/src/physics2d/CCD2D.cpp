/* Start Header *****************************************************************/
/*!
\file   CCD2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Speculative contact expansion pass for fast-moving bodies.
*/
/* End Header *******************************************************************/

#include "physics2d/CCD2D.h"

namespace Engine::Physics2D {
    /**
     * @brief Expand shallow contacts for high relative velocity pairs.
     */
    void CCD2D::ApplySpeculativeContacts(
        const PhysicsConfig2D& config,
        std::vector<BodyRuntime2D>& bodies,
        std::vector<ContactConstraint2D>& contacts) const
    {
        for (auto& c : contacts) {
            if (c.IsTrigger) {
                continue;
            }
            BodyRuntime2D& a = bodies[c.BodyA];
            BodyRuntime2D& b = bodies[c.BodyB];
            if (!a.Velocity && !b.Velocity) {
                continue;
            }
            const Vector2D va = a.Velocity ? a.Velocity->Value : Vector2D(0.0f, 0.0f);
            const Vector2D vb = b.Velocity ? b.Velocity->Value : Vector2D(0.0f, 0.0f);
            const Vector2D rel = vb - va;
            const float speed = rel.Length();
            if (speed < config.SpeculativeCCDVelocityThreshold) {
                continue;
            }

            // Expand shallow contacts slightly at high speed to reduce tunneling risk.
            c.Manifold.penetration = std::max(c.Manifold.penetration, speed * config.FixedDeltaSeconds * 0.1f);
        }
    }
}
