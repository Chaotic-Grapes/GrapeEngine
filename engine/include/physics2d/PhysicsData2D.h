/* Start Header *****************************************************************/
/*!
\file   PhysicsData2D.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Shared data structures passed between 2D physics pipeline modules.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_PHYSDATA2D_H
#define ENGINE_PHYSICS2D_PHYSDATA2D_H

#include "ecs/Entity.h"
#include "ecs/Components.h"
#include "physics/Collision.h"
#include <cstdint>
#include <memory>
#include <vector>

class TileMap;

namespace Engine::Physics2D {
    enum class ShapeType2D : uint8_t {
        None = 0,
        Circle = 1,
        Box = 2
    };

    /**
     * @brief Broadphase candidate pair produced from tree overlap query.
     */
    struct BroadphasePair2D {
        size_t BodyA = 0;
        size_t BodyB = 0;
        PackedEntityId PackedA = 0;
        PackedEntityId PackedB = 0;
    };

    /**
     * @brief Narrowphase result consumed by solver and event dispatch.
     */
    struct ContactConstraint2D {
        size_t BodyA = 0;
        size_t BodyB = 0;
        PackedEntityId PackedA = 0;
        PackedEntityId PackedB = 0;
        uint32_t FeatureId = 0;
        bool IsTrigger = false;
        bool TriggerA = false;
        bool TriggerB = false;
        float NormalImpulse = 0.0f;
        float TangentImpulse = 0.0f;
        Engine::Collision::ContactManifold Manifold{};
    };

    /**
     * @brief Runtime ECS/body cache entry used across physics stages.
     */
    struct BodyRuntime2D {
        ECS::Entity Entity{};
        PackedEntityId Packed = 0;
        ECS::Components::LocalTransform* Local = nullptr;
        ECS::Components::Rigidbody2D* Rigidbody = nullptr;
        ECS::Components::LinearVelocity2D* Velocity = nullptr;
        ECS::Components::AngularVelocity2D* AngularVelocity = nullptr;
        ECS::Components::PhysicsMaterial2D Material{};
        const ECS::Components::CircleCollider2D* Circle = nullptr;
        const ECS::Components::BoxCollider2D* Box = nullptr;
        ShapeType2D Shape = ShapeType2D::None;
        uint32_t ShapePayloadIndex = 0;
        uint16_t LayerId = 0;
        uint32_t LayerMask = 0xFFFFFFFFu;
        bool IsDynamic = false;
        bool IsSleeping = false;
        bool IsTrigger = false;
        float SleepTimer = 0.0f;
        float CachedFriction = 0.2f;
        float CachedRestitution = 0.0f;
        float CachedPositionCorrectPercent = 0.2f;
        float InverseMass = 0.0f;
        float Inertia = 0.0f;
        float InverseInertia = 0.0f;

        Engine::WorldCircle WorldCircle{};
        Engine::WorldOBB WorldObb{};
        Engine::WorldAABB WorldAabb{};
    };

    /**
     * @brief Tilemap collision data passed into PhysicsWorld2D step.
     */
    struct TilemapCollisionProxy2D {
        std::shared_ptr<TileMap> Map;
        Vector2D Origin{ 0.0f, 0.0f };
        uint16_t LayerId = 0;
        bool Enabled = false;
        PackedEntityId SourcePackedEntity = 0;
    };

    /**
     * @brief Ray cast input payload.
     */
    struct RayCastInput2D {
        Vector2D Origin{ 0.0f, 0.0f };
        Vector2D Direction{ 1.0f, 0.0f };
        float MaxDistance = 1000.0f;
    };

    /**
     * @brief Ray cast hit result.
     */
    struct RayCastHit2D {
        bool Hit = false;
        ECS::Entity Entity{};
        Vector2D Point{ 0.0f, 0.0f };
        Vector2D Normal{ 0.0f, 0.0f };
        float Distance = 0.0f;
    };
}

#endif
