/* Start Header *****************************************************************/
/*!
\file   PhysicsWorld2D.cpp
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Fixed-step 2D physics pipeline orchestration and ECS sync.
*/
/* End Header *******************************************************************/

#include "physics2d/PhysicsWorld2D.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/events/EventDispatcher.h"
#include "helpers/EntityUtils.h"
#include "helpers/TransformUtils.h"
#include "math/Vector3D.h"
#include "physics/Physics.h"
#include "physics/LayerMask.h"
#include "scene/LayerManager.h"
#include "core/World/TileMap.hpp"
#include "core/World/TileTypes.hpp"
#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cmath>

namespace {
    constexpr uint8_t kCollisionMaskBottomLeft = 1u << 0;
    constexpr uint8_t kCollisionMaskBottomRight = 1u << 1;
    constexpr uint8_t kCollisionMaskTopLeft = 1u << 2;
    constexpr uint8_t kCollisionMaskTopRight = 1u << 3;

    /**
     * @brief Resolve effective world transform for an entity.
     */
    static bool GetPhysicsWorldTransform(ECS::World& world, const ECS::Entity entity, ECS::Components::LocalTransform& outTransform) {
        const auto* localTransform = world.TryGet<ECS::Components::LocalTransform>(entity);
        if (!localTransform) {
            if (!world.Has<ECS::Components::WorldTransform>(entity)) {
                return false;
            }
            const auto& worldTransform = world.Get<ECS::Components::WorldTransform>(entity);
            TransformUtils::DecomposeTRS(worldTransform.Matrix, outTransform.Position, outTransform.Rotation, outTransform.Scale);
            return true;
        }

        Matrix4x4 worldMatrix = TransformUtils::MakeTRS(localTransform->Position, localTransform->Rotation, localTransform->Scale);
        ECS::Entity parent = world.ParentOf(entity);
        while (!parent.IsNull()) {
            // Walk up hierarchy to reconstruct world transform when only local data exists.
            if (const auto* parentLocal = world.TryGet<ECS::Components::LocalTransform>(parent)) {
                const Matrix4x4 parentMatrix = TransformUtils::MakeTRS(parentLocal->Position, parentLocal->Rotation, parentLocal->Scale);
                worldMatrix = parentMatrix * worldMatrix;
            }
            else if (world.Has<ECS::Components::WorldTransform>(parent)) {
                const auto& parentWorld = world.Get<ECS::Components::WorldTransform>(parent);
                worldMatrix = parentWorld.Matrix * worldMatrix;
                break;
            }
            else {
                break;
            }
            parent = world.ParentOf(parent);
        }
        TransformUtils::DecomposeTRS(worldMatrix, outTransform.Position, outTransform.Rotation, outTransform.Scale);
        return true;
    }

    /**
     * @brief Rebuild cached world-shape data for one runtime body.
     */
    static bool RebuildBodyWorldShape(ECS::World& world, Engine::Physics2D::BodyRuntime2D& body) {
        ECS::Components::LocalTransform worldTransform{};
        if (!GetPhysicsWorldTransform(world, body.Entity, worldTransform)) {
            return false;
        }

        if (body.Shape == Engine::Physics2D::ShapeType2D::Circle && body.Circle) {
            body.WorldCircle = Engine::Physics::GetWorldCircle(*body.Circle, worldTransform);
            body.WorldAabb.Center = body.WorldCircle.Center;
            body.WorldAabb.HalfExtents = Vector2D(body.WorldCircle.Radius, body.WorldCircle.Radius);
            return true;
        }
        if (body.Shape == Engine::Physics2D::ShapeType2D::Box && body.Box) {
            body.WorldObb = Engine::Physics::GetWorldOBB(*body.Box, worldTransform);
            body.WorldAabb = Engine::Physics::GetWorldAABB(*body.Box, worldTransform);
            return true;
        }
        return false;
    }

    /**
     * @brief 2D dot product helper.
     */
    static float Dot2D(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    /**
     * @brief Build a single-point manifold for OBB-vs-OBB overlap.
     */
    static Engine::Collision::ContactManifold TestBoxBox(
        const Engine::WorldOBB& obbA,
        const Engine::WorldOBB& obbB)
    {
        Engine::Collision::ContactManifold manifold{};
        manifold.pointCount = 0;

        const Vector2D delta = obbB.Center - obbA.Center;
        const Vector2D axes[4] = { obbA.AxisX, obbA.AxisY, obbB.AxisX, obbB.AxisY };

        float minOverlap = FLT_MAX;
        Vector2D bestAxis{ 0.0f, 0.0f };
        for (const auto& axis : axes) {
            const float rA =
                obbA.HalfExtents.X * std::abs(Dot2D(axis, obbA.AxisX)) +
                obbA.HalfExtents.Y * std::abs(Dot2D(axis, obbA.AxisY));
            const float rB =
                obbB.HalfExtents.X * std::abs(Dot2D(axis, obbB.AxisX)) +
                obbB.HalfExtents.Y * std::abs(Dot2D(axis, obbB.AxisY));
            const float dist = std::abs(Dot2D(delta, axis));
            const float overlap = rA + rB - dist;
            if (overlap <= 0.0f) {
                return manifold;
            }
            if (overlap < minOverlap) {
                minOverlap = overlap;
                bestAxis = axis;
                if (Dot2D(delta, axis) < 0.0f) {
                    bestAxis = bestAxis * -1.0f;
                }
            }
        }

        manifold.normal = bestAxis;
        manifold.penetration = minOverlap;
        auto supportPoint = [](const Engine::WorldOBB& obb, const Vector2D& direction) {
            const float signX = (Dot2D(direction, obb.AxisX) >= 0.0f) ? 1.0f : -1.0f;
            const float signY = (Dot2D(direction, obb.AxisY) >= 0.0f) ? 1.0f : -1.0f;
            return obb.Center +
                obb.AxisX * (obb.HalfExtents.X * signX) +
                obb.AxisY * (obb.HalfExtents.Y * signY);
            };

        const Vector2D pA = supportPoint(obbA, manifold.normal);
        const Vector2D pB = supportPoint(obbB, manifold.normal * -1.0f);
        manifold.points[0] = (pA + pB) * 0.5f;
        manifold.pointCount = 1;
        return manifold;
    }

    /**
     * @brief Circle-vs-box overlap helper used for tile and body contacts.
     */
    static bool TestCircleBox(
        const Engine::WorldCircle& circle,
        const Engine::WorldOBB& box,
        Vector2D& outNormal,
        float& outDepth,
        Vector2D& outContact)
    {
        if (std::abs(box.Rotation) < 1e-6f) {
            const Engine::Collision::AABB aabb = Engine::Collision::MakeAABBCenterSize(
                box.Center, box.HalfExtents * 2.0f
            );
            const Engine::Collision::Circle circ{ circle.Center, circle.Radius };
            Engine::Collision::Manifold manifold{};
            if (Engine::Collision::Overlap(circ, aabb, &manifold)) {
                outNormal = manifold.Normal;
                outDepth = manifold.Penetration;
                outContact = manifold.Contact;
                return true;
            }
            return false;
        }

        const Vector2D toCircle = circle.Center - box.Center;
        const Vector2D localCenter{
            Dot2D(toCircle, box.AxisX),
            Dot2D(toCircle, box.AxisY)
        };
        const Engine::Collision::Circle localCircle{ localCenter, circle.Radius };
        const Engine::Collision::AABB localBox = Engine::Collision::MakeAABBCenterSize(
            Vector2D{ 0.0f, 0.0f },
            Vector2D{ box.HalfExtents.X * 2.0f, box.HalfExtents.Y * 2.0f }
        );

        Engine::Collision::Manifold localManifold{};
        if (!Engine::Collision::Overlap(localCircle, localBox, &localManifold)) {
            return false;
        }

        outNormal = box.AxisX * localManifold.Normal.X + box.AxisY * localManifold.Normal.Y;
        outDepth = localManifold.Penetration;
        outContact = box.Center +
            box.AxisX * localManifold.Contact.X +
            box.AxisY * localManifold.Contact.Y;
        return true;
    }
}

namespace Engine::Physics2D {
    /**
     * @brief Sync physics runtime cache from ECS components.
     */
    void PhysicsWorld2D::SyncFromECS(ECS::World& world, Scenes::LayerManager& layerManager) {
        m_bodies.clear();
        world.Each<ECS::Components::LocalTransform>([&](const ECS::Entity e, ECS::Components::LocalTransform&) {
            if (!world.IsAlive(e) || !world.IsActiveInHierarchy(e)) {
                return;
            }
            const auto* circle = world.TryGet<ECS::Components::CircleCollider2D>(e);
            const auto* box = world.TryGet<ECS::Components::BoxCollider2D>(e);
            if (!circle && !box) {
                return;
            }

            BodyRuntime2D body{};
            body.Entity = e;
            body.Packed = ECS::EntityUtils::Pack(e);
            body.Local = world.TryGet<ECS::Components::LocalTransform>(e);
            body.Rigidbody = world.TryGet<ECS::Components::Rigidbody2D>(e);
            body.Velocity = world.TryGet<ECS::Components::LinearVelocity2D>(e);
            body.AngularVelocity = world.TryGet<ECS::Components::AngularVelocity2D>(e);
            body.Circle = circle;
            body.Box = box;
            body.Shape = circle ? ShapeType2D::Circle : ShapeType2D::Box;
            body.Material = world.Has<ECS::Components::PhysicsMaterial2D>(e) ?
                world.Get<ECS::Components::PhysicsMaterial2D>(e) :
                ECS::Components::PhysicsMaterial2D{};
            body.IsDynamic = body.Rigidbody && body.Velocity && body.Rigidbody->Mass > 0.0f;
            body.IsTrigger = (circle && (circle->Flags & 0x1u)) || (box && (box->Flags & 0x1u));
            const auto* layer = world.TryGet<ECS::Components::Layer>(e);
            body.LayerId = layer ? layer->Id : 0;
            if (!layerManager.Get(body.LayerId).physicsEnabled) {
                return;
            }
            const uint32_t runtimeLayerMask = layerManager.GetLayerMask(body.LayerId);
            const uint32_t colliderMask = (body.Shape == ShapeType2D::Circle && body.Circle) ? body.Circle->LayerMask :
                ((body.Shape == ShapeType2D::Box && body.Box) ? body.Box->LayerMask : 0xFFFFFFFFu);
            // Runtime layer matrix and per-collider mask must both allow interaction.
            body.LayerMask = runtimeLayerMask & colliderMask;

            if (!RebuildBodyWorldShape(world, body)) {
                return;
            }

            m_bodies.push_back(body);
        });

        std::sort(m_bodies.begin(), m_bodies.end(), [](const BodyRuntime2D& a, const BodyRuntime2D& b) {
            return a.Packed < b.Packed;
            });

        for (uint32_t i = 0; i < static_cast<uint32_t>(m_bodies.size()); ++i) {
            const ECS::Entity e = m_bodies[i].Entity;
            if (!world.Has<ECS::Components::PhysicsBodyHandle2D>(e)) {
                world.Add<ECS::Components::PhysicsBodyHandle2D>(e, ECS::Components::PhysicsBodyHandle2D{ i });
            }
            else {
                world.Get<ECS::Components::PhysicsBodyHandle2D>(e).BodyIndex = i;
            }
            if (!world.Has<ECS::Components::PhysicsActiveTag>(e)) {
                world.Add<ECS::Components::PhysicsActiveTag>(e, ECS::Components::PhysicsActiveTag{ true });
            }
            else {
                world.Get<ECS::Components::PhysicsActiveTag>(e).Enabled = true;
            }
        }
    }

    /**
     * @brief Recompute world-space collider data for all cached bodies.
     */
    void PhysicsWorld2D::BuildWorldShapes(ECS::World& world) {
        for (auto& body : m_bodies) {
            (void)RebuildBodyWorldShape(world, body);
        }
    }

    /**
     * @brief Integrate velocities and transforms for dynamic bodies.
     */
    void PhysicsWorld2D::IntegrateDynamic(float fixedDt) {
        for (auto& body : m_bodies) {
            if (!body.IsDynamic || !body.Local || !body.Rigidbody || !body.Velocity) {
                continue;
            }
            Vector2D acc = Engine::Physics::CalculateAcceleration(*body.Rigidbody, *body.Velocity);
            if (body.Rigidbody->Flags & (1u << 1)) {
                acc += Engine::Physics::GetGravity() * body.Rigidbody->GravityScale;
            }
            body.Velocity->Value += acc * fixedDt;
            body.Local->Position.X += body.Velocity->Value.X * fixedDt;
            body.Local->Position.Y += body.Velocity->Value.Y * fixedDt;
            if (body.AngularVelocity && !(body.Rigidbody->Flags & (1u << 2))) {
                const float angAcc = Engine::Physics::CalculateAngularAcceleration(*body.Rigidbody, *body.AngularVelocity);
                body.AngularVelocity->Value += angAcc * fixedDt;
                body.Local->Rotation = Quaternion::FromEulerRad(0.0f, 0.0f, body.AngularVelocity->Value * fixedDt) * body.Local->Rotation;
            }
        }
    }

    /**
     * @brief Resolve dynamic body collisions against tilemap collision proxies.
     */
    void PhysicsWorld2D::ResolveTilemaps(ECS::World& world, Scenes::LayerManager& layerManager, const std::vector<TilemapCollisionProxy2D>& tilemaps) {
        constexpr float kTileCoordEpsilon = 1e-4f;
        const ECS::Components::PhysicsMaterial2D tileMaterial{ 0.2f, 0.5f, 0.5f };
        for (auto& body : m_bodies) {
            if (!body.IsDynamic || body.IsTrigger || !body.Velocity || !body.Local || !body.Rigidbody) {
                continue;
            }
            for (const auto& tilemap : tilemaps) {
                if (!tilemap.Enabled || !tilemap.Map || tilemap.Map->LayerCount() == 0) {
                    continue;
                }
                if (!layerManager.Get(tilemap.LayerId).physicsEnabled) {
                    continue;
                }
                const uint32_t tileLayerMask = layerManager.GetLayerMask(tilemap.LayerId);
                if (!Engine::CanCollide(body.LayerMask, body.LayerId, tileLayerMask, tilemap.LayerId)) {
                    continue;
                }

                const float tileSize = tilemap.Map->TileSize();
                if (tileSize <= 0.0f) {
                    continue;
                }
                const float tileHalf = tileSize * 0.5f;
                const float subHalf = tileSize * 0.25f;
                const Vector2D subHalfExtents(subHalf, subHalf);
                const float minX = body.WorldAabb.Center.X - body.WorldAabb.HalfExtents.X;
                const float maxX = body.WorldAabb.Center.X + body.WorldAabb.HalfExtents.X;
                const float minY = body.WorldAabb.Center.Y - body.WorldAabb.HalfExtents.Y;
                const float maxY = body.WorldAabb.Center.Y + body.WorldAabb.HalfExtents.Y;
                int32_t tileMinX = tilemap.Map->WorldToTileSigned(minX - tilemap.Origin.X);
                int32_t tileMinY = tilemap.Map->WorldToTileSigned(minY - tilemap.Origin.Y);
                int32_t tileMaxX = tilemap.Map->WorldToTileSigned(maxX - tilemap.Origin.X - kTileCoordEpsilon);
                int32_t tileMaxY = tilemap.Map->WorldToTileSigned(maxY - tilemap.Origin.Y - kTileCoordEpsilon);
                if (tileMaxX < tileMinX) std::swap(tileMaxX, tileMinX);
                if (tileMaxY < tileMinY) std::swap(tileMaxY, tileMinY);

                const ECS::Components::PhysicsMaterial2D mat{
                    (body.Material.Friction + tileMaterial.Friction) * 0.5f,
                    std::max(body.Material.Restitution, tileMaterial.Restitution),
                    (body.Material.PositionCorrectPercent + tileMaterial.PositionCorrectPercent) * 0.5f
                };

                auto resolveTileCell = [&](const Vector2D& cellCenter, const Vector2D& halfExtents) {
                    Engine::WorldOBB tileObb{};
                    tileObb.Center = cellCenter;
                    tileObb.HalfExtents = halfExtents;
                    tileObb.Rotation = 0.0f;
                    tileObb.AxisX = Vector2D(1.0f, 0.0f);
                    tileObb.AxisY = Vector2D(0.0f, 1.0f);

                    Engine::Collision::ContactManifold manifold{};
                    if (body.Shape == ShapeType2D::Circle) {
                        Vector2D n{};
                        float depth = 0.0f;
                        Vector2D contact{};
                        if (!TestCircleBox(body.WorldCircle, tileObb, n, depth, contact)) {
                            return;
                        }
                        manifold.normal = n;
                        manifold.penetration = depth;
                        manifold.points[0] = contact;
                        manifold.pointCount = 1;
                    }
                    else {
                        manifold = TestBoxBox(body.WorldObb, tileObb);
                        if (manifold.pointCount <= 0) {
                            return;
                        }
                    }

                    ECS::Components::Rigidbody2D staticRb{};
                    staticRb.Mass = 0.0f;
                    ECS::Components::LinearVelocity2D staticVel{};
                    ECS::Components::LocalTransform staticXf{};
                    staticXf.Position = { cellCenter.X, cellCenter.Y, 0.0f };
                    staticXf.Scale = { 1.0f, 1.0f, 1.0f };
                    staticXf.Rotation = Quaternion::Identity();

                    ECS::Components::Rigidbody2D dynamicRb = *body.Rigidbody;
                    ECS::Components::LinearVelocity2D dynamicVel = *body.Velocity;
                    Engine::Physics::ResolveCollisionManifold(dynamicRb, staticRb, dynamicVel, staticVel, *body.Local, staticXf, manifold, mat);
                    *body.Velocity = dynamicVel;
                    // Keep derived world-shape cache in sync after each tile response.
                    (void)RebuildBodyWorldShape(world, body);
                };

                for (int32_t ty = tileMinY; ty <= tileMaxY; ++ty) {
                    for (int32_t tx = tileMinX; tx <= tileMaxX; ++tx) {
                        if (tilemap.Map->GetTileSigned(0, tx, ty) == EMPTY_TILE) {
                            continue;
                        }

                        const uint8_t mask = static_cast<uint8_t>(tilemap.Map->GetCollisionMaskSigned(tx, ty) & 0x0Fu);
                        if (mask == 0u) {
                            continue;
                        }

                        const float tileWorldX = tilemap.Origin.X + tilemap.Map->TileToWorldSigned(tx);
                        const float tileWorldY = tilemap.Origin.Y + tilemap.Map->TileToWorldSigned(ty);

                        if (mask == 0x0Fu) {
                            // Fully solid tile.
                            resolveTileCell(Vector2D(tileWorldX + tileHalf, tileWorldY + tileHalf), Vector2D(tileHalf, tileHalf));
                            continue;
                        }
                        if (mask == (kCollisionMaskBottomLeft | kCollisionMaskBottomRight)) {
                            resolveTileCell(Vector2D(tileWorldX + tileHalf, tileWorldY + subHalf), Vector2D(tileHalf, subHalf));
                            continue;
                        }
                        if (mask == (kCollisionMaskTopLeft | kCollisionMaskTopRight)) {
                            resolveTileCell(Vector2D(tileWorldX + tileHalf, tileWorldY + tileSize - subHalf), Vector2D(tileHalf, subHalf));
                            continue;
                        }
                        if (mask == (kCollisionMaskBottomLeft | kCollisionMaskTopLeft)) {
                            resolveTileCell(Vector2D(tileWorldX + subHalf, tileWorldY + tileHalf), Vector2D(subHalf, tileHalf));
                            continue;
                        }
                        if (mask == (kCollisionMaskBottomRight | kCollisionMaskTopRight)) {
                            resolveTileCell(Vector2D(tileWorldX + tileSize - subHalf, tileWorldY + tileHalf), Vector2D(subHalf, tileHalf));
                            continue;
                        }

                        if (mask & kCollisionMaskBottomLeft) {
                            // Corner masks resolve as quarter tiles.
                            resolveTileCell(Vector2D(tileWorldX + subHalf, tileWorldY + subHalf), subHalfExtents);
                        }
                        if (mask & kCollisionMaskBottomRight) {
                            resolveTileCell(Vector2D(tileWorldX + tileSize - subHalf, tileWorldY + subHalf), subHalfExtents);
                        }
                        if (mask & kCollisionMaskTopLeft) {
                            resolveTileCell(Vector2D(tileWorldX + subHalf, tileWorldY + tileSize - subHalf), subHalfExtents);
                        }
                        if (mask & kCollisionMaskTopRight) {
                            resolveTileCell(Vector2D(tileWorldX + tileSize - subHalf, tileWorldY + tileSize - subHalf), subHalfExtents);
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Update body sleeping state from velocity thresholds.
     */
    void PhysicsWorld2D::UpdateSleepState(float fixedDt) {
        for (auto& body : m_bodies) {
            if (!body.IsDynamic || !body.Velocity || !body.AngularVelocity) {
                continue;
            }
            const bool lowLinear = body.Velocity->Value.Length() < m_config.SleepLinearThreshold;
            const bool lowAngular = std::abs(body.AngularVelocity->Value) < m_config.SleepAngularThreshold;
            if (lowLinear && lowAngular) {
                body.SleepTimer += fixedDt;
                if (body.SleepTimer >= m_config.SleepTimeThresholdSeconds) {
                    body.IsSleeping = true;
                }
            }
            else {
                body.SleepTimer = 0.0f;
                body.IsSleeping = false;
            }
        }
    }

    /**
     * @brief Write runtime sleep/contact state back to ECS.
     */
    void PhysicsWorld2D::WriteBackToECS(ECS::World& world) {
        for (const auto& body : m_bodies) {
            if (!world.IsAlive(body.Entity)) {
                continue;
            }
            if (body.IsDynamic) {
                if (!world.Has<ECS::Components::SleepingTag>(body.Entity)) {
                    world.Add<ECS::Components::SleepingTag>(body.Entity, ECS::Components::SleepingTag{ body.IsSleeping });
                }
                else {
                    world.Get<ECS::Components::SleepingTag>(body.Entity).Sleeping = body.IsSleeping;
                }
            }
            if (!world.Has<ECS::Components::PhysicsContactCache2D>(body.Entity)) {
                world.Add<ECS::Components::PhysicsContactCache2D>(body.Entity, ECS::Components::PhysicsContactCache2D{ 0 });
            }
        }
    }

    /**
     * @brief Execute a full fixed simulation step.
     */
    void PhysicsWorld2D::StepFixed(
        ECS::World& world,
        Scenes::LayerManager& layerManager,
        ECS::Events::EventDispatcher& dispatcher,
        float fixedDt,
        const std::vector<TilemapCollisionProxy2D>& tilemaps)
    {
        // Fixed step currently comes from config; external fixedDt is intentionally ignored.
        (void)fixedDt;
        m_stats.ResetFrame();
        const auto t0 = std::chrono::high_resolution_clock::now();
        SyncFromECS(world, layerManager);
        BuildWorldShapes(world);
        IntegrateDynamic(m_config.FixedDeltaSeconds);
        const auto t1 = std::chrono::high_resolution_clock::now();

        BuildWorldShapes(world);
        m_broadphase.Build(m_bodies, m_config.FatAABBPadding);
        m_pairs = m_broadphase.GeneratePairsDeterministicParallel(m_bodies);
        m_stats.BroadphaseTaskCount = std::min<uint32_t>(8u, static_cast<uint32_t>(std::max<size_t>(1, m_bodies.size())));
        const auto t2 = std::chrono::high_resolution_clock::now();

        m_contacts = m_narrowphase.BuildContactsParallel(m_bodies, m_pairs);
        m_stats.NarrowphaseTaskCount = std::min<uint32_t>(8u, static_cast<uint32_t>(std::max<size_t>(1, m_pairs.size())));
        m_ccd.ApplySpeculativeContacts(m_config, m_bodies, m_contacts);
        m_islands = m_islandBuilder.Build(m_bodies, m_contacts);
        const auto t3 = std::chrono::high_resolution_clock::now();

        m_solver.Solve(m_config, m_bodies, m_contacts, m_islands);
        m_stats.SolverTaskCount = std::min<uint32_t>(8u, static_cast<uint32_t>(std::max<size_t>(1, m_islands.size())));
        ResolveTilemaps(world, layerManager, tilemaps);
        const auto t4 = std::chrono::high_resolution_clock::now();

        UpdateSleepState(m_config.FixedDeltaSeconds);
        const auto t5 = std::chrono::high_resolution_clock::now();

        m_contactManager.EmitEvents(dispatcher, world, m_contacts);
        const auto t6 = std::chrono::high_resolution_clock::now();

        WriteBackToECS(world);
        const auto t7 = std::chrono::high_resolution_clock::now();

        m_stats.BodyCount = static_cast<uint32_t>(m_bodies.size());
        m_stats.PairCount = static_cast<uint32_t>(m_pairs.size());
        m_stats.ManifoldCount = static_cast<uint32_t>(m_contacts.size());
        m_stats.ConstraintCount = static_cast<uint32_t>(m_contacts.size());
        for (const auto& b : m_bodies) {
            if (b.IsDynamic) ++m_stats.ActiveBodyCount;
            if (b.IsSleeping) ++m_stats.SleepingBodyCount;
        }

        auto ms = [](auto a, auto b) {
            return std::chrono::duration<float, std::milli>(b - a).count();
            };
        m_stats.SyncMs = ms(t0, t1);
        m_stats.BroadphaseMs = ms(t1, t2);
        m_stats.NarrowphaseMs = ms(t2, t3);
        m_stats.SolveMs = ms(t3, t4);
        m_stats.SleepMs = ms(t4, t5);
        m_stats.EventsMs = ms(t5, t6);
        m_stats.WritebackMs = ms(t6, t7);

        if (m_debugDraw) {
            for (const auto& body : m_bodies) {
                const Vector2D mn = body.WorldAabb.Center - body.WorldAabb.HalfExtents;
                const Vector2D mx = body.WorldAabb.Center + body.WorldAabb.HalfExtents;
                m_debugDraw->DrawAABB(mn, mx);
            }
            for (const auto& c : m_contacts) {
                if (c.Manifold.pointCount > 0) {
                    m_debugDraw->DrawContactPoint(c.Manifold.points[0], c.Manifold.normal);
                }
            }
        }
    }
}
