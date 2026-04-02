/* Start Header *****************************************************************/
/*!
\file   RendererSystem.cpp
\author Choi Meng Yew (90%)
        Foo Rui Qin (10%)
\date   12th March 2026
\par    choi.m@digipen.edu
        ruiqin.foo@digipen.edu
\brief
Implementation of the RendererSystem, the high-level rendering pipeline
for the ECS. Manages shader programs, framebuffers, and the RenderGraph
to orchestrate a multi-pass pipeline including scene rendering, HDR,
bloom extraction, two-pass Gaussian blur, and final tone-mapped composite.

Responsibilities:
- Initialize and manage rendering resources (shaders, render targets, framebuffers)
- Execute a RenderGraph-based pipeline for HDR and post-processing effects
- Render ECS entities by layer with support for SDF primitives, sprites, and text
- Handle object picking and selection highlighting via ID-encoded FBO
- Support external camera injection for flexible rendering contexts

The RendererSystem acts as the bridge between ECS data and GPU rendering,
handling batching, shader bindings, and visual effects in a modular,
pass-based architecture.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

// ============================================================================
// Engine Systems
// ============================================================================
#include "ecs/systems/BoidSystem.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/RendererSystem.h"

// ============================================================================
// Core Engine
// ============================================================================
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "core/Logger.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "services/Input.h"

// ============================================================================
// Graphics
// ============================================================================
#include "graphics/renderer.hpp"
#include "graphics/texture.hpp"
#include "graphics/RenderGraph.hpp"
#include "graphics/PixelBufferObject.hpp"
#include "graphics/font.hpp"
#include "graphics/LightManager.hpp"

// ============================================================================
// ECS Components
// ============================================================================
#include "ecs/Components.h"

// ============================================================================
// Services
// ============================================================================
#include "services/TimeSystem.h"
#include "services/ResourceManager.h"
#include "platform/IPlatformContext.h"

// ============================================================================
// Helpers
// ============================================================================
#include "helpers/TransformUtils.h"
#include "core/World/TileTypes.hpp"
#include "ecs/StringTable.h"
#include "physics2d/internal/ParallelFor.h"

// ============================================================================
// Standard Library
// ============================================================================
#include <algorithm>
#include <iterator>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cmath>
#include <filesystem>

// ============================================================================
// Third-Party Libraries
// ============================================================================
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {
    constexpr uint32_t kRendererPrepMaxWorkers = 8u;
    constexpr size_t kRendererLightParallelThreshold = 128u;
    constexpr size_t kRendererBucketParallelThreshold = 512u;

    // Resolve a project-relative path to an absolute path for loading assets (for existing maps)
    std::string ResolveProjectPathForLoad(const std::string& path) {
        if (path.empty() || !Engine::ProjectPaths::IsInitialized()) {
            return path;
        }

        // Only convert to absolute if the path is relative, otherwise assume it's already absolute to avoid messing with non-project paths
        std::filesystem::path fsPath(path);
        if (fsPath.is_absolute()) {
            return path;
        }

        // Convert to absolute path based on project directory and normalize it to remove redundant components
        std::filesystem::path absolute = Engine::ProjectPaths::ToAbsolutePath(path);
        return absolute.lexically_normal().string();
    }

    // Build a tileset from a texture by slicing it into a grid of tile UVs based on the specified tile pixel size
    std::shared_ptr<Tileset> BuildTilesetFromTexture(const std::string& texturePath, uint32_t tilePixelSize) {
        // Load the texture using RM
        auto texture = RM.Get<Texture>(texturePath);
        if (!texture) {
            LOG_WARNING("[TileMap] Failed to load tileset texture: " << texturePath);
            return nullptr;
        }

        // Compute the number of columns and rows of tiles based on the texture dimensions and tile pixel size
        const uint32_t texWidth = static_cast<uint32_t>(texture->Width());
        const uint32_t texHeight = static_cast<uint32_t>(texture->Height());
        const uint32_t tilePx = std::max(1u, tilePixelSize);
        const uint32_t cols = texWidth / tilePx;
        const uint32_t rows = texHeight / tilePx;

        // If the texture is too small to fit even one tile, log a warning and return null to indicate failure
        if (cols == 0 || rows == 0) {
            LOG_WARNING("[TileMap] Tileset texture too small for tile size: " << texturePath);
            return nullptr;
        }

        // Create tileset
        // Tile IDs are assigned sequentially starting from 0
        auto tileset = std::make_shared<Tileset>(static_cast<uint32_t>(texture->ID()));
        TileID id = 0;

        // Iterate over the grid of tiles in row-major order and compute UV coordinates for each tile
        for (uint32_t row = 0; row < rows; row++) {
            // For each tile, compute the UV coordinates based on its position in the texture
            for (uint32_t col = 0; col < cols; col++) {
                // Tile IDs are limited to 12 bits (see TileTypes.hpp), so if we exceed that, we stop defining more tiles
                if (id >= TILE_ID_MASK) {
                    return tileset;
                }

                // Compute UVs with row 0 at the top of the texture
                // OpenGL expects (0,0) at the bottom-left, so we flip the V coordinate
                const float u0 = static_cast<float>(col * tilePx) / static_cast<float>(texWidth);
                const float u1 = static_cast<float>((col + 1) * tilePx) / static_cast<float>(texWidth);
                const float v1 = 1.0f - static_cast<float>(row * tilePx) / static_cast<float>(texHeight);
                const float v0 = 1.0f - static_cast<float>((row + 1) * tilePx) / static_cast<float>(texHeight);

                // Define the tile in the tileset with the computed UVs and no collision by default
                TileUV uv{ u0, v0, u1, v1 };
                tileset->DefineTile(id, uv, CollisionType::NONE);
                id++;
            }
        }
        // Return the constructed tileset
        return tileset;
    }

}

namespace ECS {
    static constexpr uint32_t INVALID_ENTITY_ID = Entity::NPOS32;

    // Define the global instance pointer (avoids dllimport issues with static class members)
    RendererSystem* g_rendererSystemInstance = nullptr;

    // Implementation of static GetInstance method
    RendererSystem* RendererSystem::GetInstance() {
        return g_rendererSystemInstance;
    }

    // Apply editor-provided GUI viewport mapping used by screen-space GUI passes
    void RendererSystem::SetGUIViewport(const Vector2D& origin, const Vector2D& size, const Vector2D& displayScale) {
        m_guiViewport.Origin = origin;
        m_guiViewport.Size = size;
        m_guiViewport.DisplayScale = displayScale;
        m_guiViewport.Active = true;
    }

    // Reset GUI viewport mapping so GUI coordinates target the full render buffer
    void RendererSystem::ResetGUIViewport() {
        m_guiViewport.Origin = { 0.0f, 0.0f };
        m_guiViewport.Size = m_renderTargetSize;
        m_guiViewport.DisplayScale = { 1.0f, 1.0f };
        if (Engine::CORE) {
            auto* context = Engine::CORE->GetPlatformContext();
            auto* window = context ? context->GetMainWindow() : nullptr;
            if (window) {
                auto* native = static_cast<GLFWwindow*>(window->GetNativeHandle());
                if (native) {
                    int windowW = 0;
                    int windowH = 0;
                    glfwGetWindowSize(native, &windowW, &windowH);
                    if (windowW > 0 && windowH > 0) {
                        m_guiViewport.DisplayScale = {
                            m_renderTargetSize.X / static_cast<float>(windowW),
                            m_renderTargetSize.Y / static_cast<float>(windowH)
                        };
                    }
                }
            }
        }
        m_guiViewport.Active = true;
    }

    // Convert world coordinates to screen space
    bool RendererSystem::WorldToScreen(World& world, const Vector3D& worldPos, const Vector2D& viewportOrigin,
        const Vector2D& viewportSize, Vector2D& outScreen) {
        glm::mat4 view(1.0f);
        glm::mat4 projection(1.0f);
        float orthoSize = kReferenceOrthoSize;
        if (!GetCameraMatrices(world, view, projection, orthoSize)) {
            return false;
        }

        const glm::vec4 clip = projection * view * glm::vec4(worldPos.X, worldPos.Y, worldPos.Z, 1.0f);
        if (std::abs(clip.w) < 1e-6f) {
            return false;
        }

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) {
            return false;
        }

        const float screenX = viewportOrigin.X + (ndc.x * 0.5f + 0.5f) * viewportSize.X;
        const float screenY = viewportOrigin.Y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.Y;
        outScreen = { screenX, screenY };
        return true;
    }

    // Return camera basis
    bool RendererSystem::GetCameraBasis(World& world, glm::vec3& outRight, glm::vec3& outUp) {
        glm::mat4 view(1.0f);
        glm::mat4 projection(1.0f);
        float orthoSize = kReferenceOrthoSize;
        if (!GetCameraMatrices(world, view, projection, orthoSize)) {
            return false;
        }

        const glm::mat4 invView = glm::inverse(view);
        outRight = glm::normalize(glm::vec3(invView[0]));
        outUp = glm::normalize(glm::vec3(invView[1]));
        return true;
    }

    static bool BuildWorldFromLocalHierarchy(
        World& world,
        const Entity entity,
        const Components::LocalTransform& local,
        Vector3D& outPosition,
        Quaternion& outRotation,
        Vector3D& outScale)
    {
        Matrix4x4 worldMatrix = TransformUtils::MakeTRS(local.Position, local.Rotation, local.Scale);
        Entity parent = world.ParentOf(entity);
        while (!parent.IsNull()) {
            if (const auto* parentLocal = world.TryGet<Components::LocalTransform>(parent)) {
                const Matrix4x4 parentMatrix = TransformUtils::MakeTRS(parentLocal->Position, parentLocal->Rotation, parentLocal->Scale);
                worldMatrix = parentMatrix * worldMatrix;
            } else if (world.Has<Components::WorldTransform>(parent)) {
                const auto& parentWorld = world.Get<Components::WorldTransform>(parent);
                worldMatrix = parentWorld.Matrix * worldMatrix;
                break;
            } else {
                break;
            }
            parent = world.ParentOf(parent);
        }

        TransformUtils::DecomposeTRS(worldMatrix, outPosition, outRotation, outScale);
        return true;
    }

    static float GetPhysicsInterpolationSecondsForFrame() {
        static int cachedFrame = -1;
        static float cachedSeconds = 0.0f;

        const int frame = TimeSystem::Instance().GetFrameCount();
        if (frame == cachedFrame) {
            return cachedSeconds;
        }

        cachedFrame = frame;
        cachedSeconds = 0.0f;

        if (!Engine::CORE) {
            return cachedSeconds;
        }

        auto* physicsSystem = Engine::CORE->GetSystemManager().GetSystem<ECS::PhysicsSystem>();
        if (!physicsSystem) {
            return cachedSeconds;
        }

        cachedSeconds = physicsSystem->GetRemainingAccumulatorSeconds();
        if (cachedSeconds < 0.0f) {
            cachedSeconds = 0.0f;
        }
        return cachedSeconds;
    }

    // Helper function to get the effective transform for rendering.
    // Dynamic physics bodies use latest LocalTransform hierarchy data to avoid stale pre-physics WorldTransform.
    static void GetRenderTransform(World& world, const Entity entity,
        const Components::LocalTransform& lt,
        Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) {
        const auto* rb = world.TryGet<Components::Rigidbody2D>(entity);
        const auto* vel = world.TryGet<Components::LinearVelocity2D>(entity);
        const bool isDynamicPhysicsBody = rb && vel && rb->Mass > 0.0f;

        if (isDynamicPhysicsBody) {
            BuildWorldFromLocalHierarchy(world, entity, lt, outPosition, outRotation, outScale);
        } else if (world.Has<Components::WorldTransform>(entity)) {
            const auto& wt = world.Get<Components::WorldTransform>(entity);
            TransformUtils::DecomposeTRS(wt.Matrix, outPosition, outRotation, outScale);
        } else {
            outPosition = lt.Position;
            outRotation = lt.Rotation;
            outScale = lt.Scale;
        }

        if (isDynamicPhysicsBody) {
            const float interpSeconds = GetPhysicsInterpolationSecondsForFrame();
            if (interpSeconds > 0.0f) {
                outPosition.X += vel->Value.X * interpSeconds;
                outPosition.Y += vel->Value.Y * interpSeconds;
                if (const auto* angVel = world.TryGet<Components::AngularVelocity2D>(entity)) {
                    if ((rb->Flags & (1u << 2)) == 0u) {
                        outRotation = Quaternion::FromEulerRad(0.0f, 0.0f, angVel->Value * interpSeconds) * outRotation;
                    }
                }
            }
        }
    }

    // Return metadata
    SystemMetadata RendererSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Renderer");
        // Note: RendererSystem reads many components (SpriteRenderer2D, WorldTransform, etc.)
        // but uses them through world iteration rather than static declaration
        // For now, marking as read-only with minimal dependency tracking
        // Full component access list documented in OnUpdate()
        builder.SetExecutionOrder(0);
        builder.SetGroup(SystemGroup::Render);
        builder.SetRunMode(SystemRunMode::Always);
        return builder.Build();
    }

    // Initialize system state
    void RendererSystem::OnCreate(World& /*world*/) {
        if (m_initialized)
            return;

        m_initialized = true;

        // Set global instance pointer
        g_rendererSystemInstance = this;

        auto* context = Engine::CORE->GetPlatformContext();
        auto* mainWindow = context ? context->GetMainWindow() : nullptr;
        if (!mainWindow) {
            LOG_ERROR("RendererSystem::OnCreate: No main window available");
            return;
        }
        const int width = mainWindow->GetWidth();
        const int height = mainWindow->GetHeight();
        m_renderTargetSize = { static_cast<float>(width), static_cast<float>(height) };
        ResetGUIViewport();
        m_windowAspectRatio = (height > 0) ? (static_cast<float>(width) / height) : 1.0f;
        m_windowAspectDirty = true;

        // Use RM instead!
        m_shader = RM.Get<Shader>("assets/shaders/batch");
        m_guiShader = RM.Get<Shader>("assets/shaders/gui");
        m_textShader = RM.Get<Shader>("assets/shaders/sdf_text");
        m_sdfCircleShader = RM.Get<Shader>("assets/shaders/sdf_circle");
        m_bloomExtractShader = RM.Get<Shader>("assets/shaders/bloom_extract");

        m_bloomBlurShader = RM.GetShader(
            "assets/shaders/bloom_extract.vert",
            "assets/shaders/bloom_blur.frag");

        m_bloomCombineShader = RM.GetShader(
            "assets/shaders/bloom_extract.vert",
            "assets/shaders/bloom_combine.frag");

        m_blitShader = RM.Get<Shader>("assets/shaders/blit");

        // Compute-related shaders
        m_boidShader = RM.Get<Shader>("assets/shaders/boid");
        m_boidSystem = Engine::CORE->GetSystemManager().GetSystem<ECS::BoidSystem>();

        // Particle-stuff
        m_particleShader = RM.Get<Shader>("assets/shaders/particle");
        m_particleSystem = Engine::CORE->GetSystemManager().GetSystem<ECS::ParticleSystem>();

        // Object Picking
        m_pickingFBO.Create(width, height, false, false, 1);
        m_pbos[0].Create(4, GL_STREAM_READ);
        m_pbos[1].Create(4, GL_STREAM_READ);

        // Renderer
        m_renderer = std::make_unique<Renderer>(15000);
        m_guiRenderer = std::make_unique<Renderer>(4000);

        // RenderGraph now owns all framebuffers (no more m_fbos!)
        m_renderGraph = std::make_unique<RenderGraph>();

        m_renderGraph->CreateTexture("HDR",
            { width, height, GL_RGBA16F, false });

        m_renderGraph->CreateTexture("Backbuffer",
            { width, height, GL_RGBA8, true });

        m_renderGraph->CreateTexture("BloomExtract"
            , { width / 2, height / 2, GL_RGBA16F, false });

        m_renderGraph->CreateTexture("BloomBlur"
            , { width / 2, height / 2, GL_RGBA16F, false });

        m_renderGraph->CreateTexture("LDR",
            { width, height, GL_RGBA8, false });

        // Resize HDR when window resizes
        Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& msg)
            {
                // TODO: Add RenderGraph::ResizeTexture() method to handle this?
                // For now, recreate the graph on resize
                m_renderGraph = std::make_unique<RenderGraph>();

                m_renderGraph->CreateTexture("HDR",          { msg.Width,      msg.Height,      GL_RGBA16F, false });
                m_renderGraph->CreateTexture("Backbuffer",   { msg.Width,      msg.Height,      GL_RGBA8,   true  });
                m_renderGraph->CreateTexture("BloomExtract", { msg.Width / 2,  msg.Height / 2,  GL_RGBA16F, false });
                m_renderGraph->CreateTexture("BloomBlur",    { msg.Width / 2,  msg.Height / 2,  GL_RGBA16F, false });
                m_renderGraph->CreateTexture("LDR",          { msg.Width,      msg.Height,      GL_RGBA8, false });

                // Update fallback projection
                m_projection = glm::ortho(
                    0.f, static_cast<float>(msg.Width),
                    0.f, static_cast<float>(msg.Height),
                    -1.f, 1.f
                );

                // Resize picking FBO
                m_pickingFBO.Resize(msg.Width, msg.Height, false, false);

                m_renderTargetSize = { static_cast<float>(msg.Width), static_cast<float>(msg.Height) };
                ResetGUIViewport();
                m_windowAspectRatio = msg.AspectRatio;
                m_windowAspectDirty = true;
            });

        // Projection matrix
        m_projection = glm::ortho(
            0.f, static_cast<float>(width),
            0.f, static_cast<float>(height),
            -1.f, 1.f
        );

        // OpenGL state
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Light manager (SSBO creation etc.)
        m_lightManager.Initialize();
    }

    // Bind world
    void RendererSystem::BindWorld(World& world) {
        (void)world; // Currently unused
    }

    // Return camera matrices
    bool RendererSystem::GetCameraMatrices(World& world, glm::mat4& outView, glm::mat4& outProjection, float& outOrthoSize) {
        // Default fallback
        outView = glm::mat4(1.0f);
        outProjection = glm::mat4(1.0f);
        outOrthoSize = kReferenceOrthoSize;

        // Use external camera if provided
        if (m_activeCamera) {
            outView = m_activeCamera->GetViewMatrix();
            outProjection = m_activeCamera->GetProjectionMatrix();
            outOrthoSize = m_activeCamera->OrthoSize;
            return true;
        }

        // Fall back to ECS camera
        bool foundActive = false;
        world.Each<ECS::Components::LocalTransform, ECS::Components::Camera3D>(
            [&](ECS::Entity e,
                const ECS::Components::LocalTransform& transform,
                const ECS::Components::Camera3D& camera)
            {
                if (foundActive || !camera.Active || !world.IsActiveInHierarchy(e)) return;

                Vector3D position{};
                Quaternion rotation{};
                if (world.Has<Components::WorldTransform>(e)) {
                    const auto& wt = world.Get<Components::WorldTransform>(e);
                    bool useWorld = true;
                    if (world.Has<Components::LocalTransform>(e)) {
                        const float localLenSq = transform.Position.X * transform.Position.X
                            + transform.Position.Y * transform.Position.Y
                            + transform.Position.Z * transform.Position.Z;
                        const bool worldAtOrigin = std::abs(wt.Matrix.m03) < 1e-4f
                            && std::abs(wt.Matrix.m13) < 1e-4f
                            && std::abs(wt.Matrix.m23) < 1e-4f;
                        if (worldAtOrigin && localLenSq > 1e-6f) {
                            useWorld = false;
                        }
                    }
                    if (useWorld) {
                        Vector3D scale;
                        // Decompose transform matrix into TRS components
                        TransformUtils::DecomposeTRS(wt.Matrix, position, rotation, scale);
                    }
                    else {
                        position = transform.Position;
                        rotation = transform.Rotation;
                    }
                }
                else {
                    position = transform.Position;
                    rotation = transform.Rotation;
                }

                const glm::vec3 eye(position.X, position.Y, position.Z);
                const glm::quat rot(rotation.W, rotation.X, rotation.Y, rotation.Z);
                const glm::vec3 forward = rot * glm::vec3(0.0f, 0.0f, -1.0f);
                const glm::vec3 up = rot * glm::vec3(0.0f, 1.0f, 0.0f);
                outView = glm::lookAt(eye, eye + forward, up);

                // Projection
                // In standalone runtime (no editor-managed viewports), render against
                // the current render-target aspect so serialized camera aspect values
                // don't distort the final image when window settings differ
                float effectiveAspect = camera.AspectRatio;
                if (!m_activeCamera && m_viewports.empty() && m_renderTargetSize.Y > 0.0f) {
                    effectiveAspect = m_renderTargetSize.X / m_renderTargetSize.Y;
                }
                if (effectiveAspect <= 0.0f) {
                    effectiveAspect = 1.0f;
                }

                if (camera.UsePerspective) {
                    // camera.FOV stored in degrees
                    outProjection = glm::perspective(
                        glm::radians(camera.FOV),
                        effectiveAspect,
                        camera.NearPlane,
                        camera.FarPlane
                    );
                }
                else {
                    const float halfH = camera.OrthoSize;
                    const float halfW = halfH * effectiveAspect;
                    outProjection = glm::ortho(
                        -halfW, +halfW,
                        -halfH, +halfH,
                        camera.NearPlane, camera.FarPlane
                    );
                }

                foundActive = true;
                outOrthoSize = camera.OrthoSize;
            }
        );

        if (foundActive) {
            return true;
        }

        // Fallback: screen-aligned ortho
        auto* context = Engine::CORE->GetPlatformContext();
        auto* mainWindow = context ? context->GetMainWindow() : nullptr;
        if (mainWindow) {
            outProjection = glm::ortho(0.f, static_cast<float>(mainWindow->GetWidth()),
                0.f, static_cast<float>(mainWindow->GetHeight()),
                -1.f, 1.f);
            return true;
        }

        return false;
    }

    // Update system state for this frame
    void RendererSystem::OnUpdate(World& world) {
        if (!m_renderer)
            return;

        auto* context = Engine::CORE->GetPlatformContext();
        auto* win = context ? context->GetMainWindow() : nullptr;
        if (!win) return;

        if (m_windowAspectDirty && m_viewports.empty() && !m_activeCamera) {
            const float aspect = (m_windowAspectRatio > 0.0f) ? m_windowAspectRatio : 1.0f;
            // Render 3D camera passes
            world.Each<ECS::Components::Camera3D>([&](ECS::Entity /*e*/, ECS::Components::Camera3D& camera)
                {
                    camera.AspectRatio = aspect;
                });
            m_windowAspectDirty = false;
        }

        // For game mode, sync the runtime tilemap cache with the current world state
        // This will add new tilemaps, update changed ones and remove deleted ones
        if (Engine::CORE->GetMode() == Engine::EngineMode::Game) {
            RefreshRuntimeTileMaps(world);
        } 
        // For editor mode, we rely on the debug tilemap list which is manually managed by the editor (no automatic syncing)
        else if (!m_runtimeTileMaps.empty()) {
            m_runtimeTileMaps.clear();
        }

        // ============================================================
        // SHARED WORK (once per frame)
        // ============================================================
        CollectLights(world);

        std::vector<std::vector<Entity>> buckets;
        int maxLayerId = -1;
        BucketEntities(world, buckets, maxLayerId);

        // ============================================================
        // MULTI-VIEWPORT RENDERING (if viewports registered)
        // ============================================================
        if (!m_viewports.empty()) {
            for (auto& vp : m_viewports) {
                if (!vp.Active || !vp.Camera) continue;
                if (vp.Size.x <= 0 || vp.Size.y <= 0) continue;

                glm::mat4 view = vp.Camera->GetViewMatrix();
                glm::mat4 proj = vp.Camera->GetProjectionMatrix();
                glm::mat4 viewProj = proj * view;

                float orthoSize = vp.Camera->OrthoSize;
                float zoomScale = kReferenceOrthoSize / orthoSize;
                float bloomRadius = (kDesiredBloomWorldSpread / kReferenceOrthoSize)
                    * (vp.Size.y / 2.0f) * zoomScale;

                RenderSceneToHDR(world, vp, viewProj, buckets, maxLayerId);
                if (vp.BloomEnabled) {
                    RenderBloom(vp, bloomRadius);
                }
                else {
                    // Keep the bloom buffer black so tone mapping and debug views remain deterministic.
                    vp.BloomExtract->BindAndClear(0, 0, 0, 1);
                    glViewport(0, 0, vp.BloomExtract->Width(), vp.BloomExtract->Height());
                    Framebuffer::Unbind();
                }
                ToneMap(vp, vp.BloomEnabled);
                RenderOverlayQuads(vp, viewProj);
                RenderWireframes(vp, viewProj);
                RenderGUI(vp);
                RenderPicking(world, vp, viewProj, buckets);
            }

            m_wireframeQueue.clear();
            m_overlayQuadQueue.clear();
            m_guiPanelQueue.clear();
            m_guiImageQueue.clear();
            m_guiTextQueue.clear();
            m_worldGuiPanelQueue.clear();
            m_worldGuiImageQueue.clear();
            m_worldGuiTextQueue.clear();

            // Unbind the current render target
            Framebuffer::Unbind();
            return;  // EXIT HERE - skip RenderGraph path
        }

        // ============================================================
        // FALLBACK: Original RenderGraph path (unchanged for backwards compatibility)
        // ============================================================

        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        m_cameraOrthoSize = kReferenceOrthoSize;

        GetCameraMatrices(world, view, projection, m_cameraOrthoSize);

        const float bloomBufferHeight = static_cast<float>(win->GetHeight()) / 2.0f;
        const float zoomScale = kReferenceOrthoSize / m_cameraOrthoSize;
        const float bloomRadiusTexels =
            (kDesiredBloomWorldSpread / kReferenceOrthoSize) *
            bloomBufferHeight * zoomScale;

        const glm::mat4 viewProj = projection * view;

        // NOTE: we remove the duplicate light collection and entity bucketing
        // since we now do it above. The RenderGraph passes can use the
        // already-populated `buckets` and `maxLayerId` variables
        // Reusable temporary buffers to avoid per-entity allocations
        std::vector<glm::vec2> transformedCorners;
        std::vector<Entity> sortedLayerEntities;
        std::vector<glm::vec2> polyPoints;

        // ============================================================
        // RENDER GRAPH SETUP
        // ============================================================
        m_renderGraph->Reset();  // Clear passes from last frame

        // Pass 1: Render scene to HDR framebuffer
        m_renderGraph->AddPass("Scene2D", {}, { "HDR" },
            [this, &world, &viewProj, &maxLayerId, &buckets, &transformedCorners, &polyPoints, &sortedLayerEntities, &win](ResourceAccessor& res)
            {
                (void)res;
                // Get HDR framebuffer from render graph
                auto* hdrFbo = res.GetFramebuffer("HDR");
                if (!hdrFbo) {
                    std::cerr << "ERROR: HDR framebuffer not found!\n";
                    return;
                }

                // Because of tone-mapping, the background will appear slightly lighter
                // I chose a slightly brighter neutral gray for a nicer look
                hdrFbo->BindAndClear(0.025f, 0.028f, 0.032f, 1.0f);

                // Get LayerManager for layer visibility and render order
                auto* layerManager = world.GetLayerManager();

                // Determine render order using LayerManager if available, otherwise fall back to manual iteration
                std::vector<uint16_t> renderOrder;
                if (layerManager) {
                    renderOrder = layerManager->DrawOrder();
                }
                else {
                    // Fallback: manually iterate from 0 to maxLayerId
                    for (int layer = 0; layer <= maxLayerId; ++layer) {
                        renderOrder.push_back(static_cast<uint16_t>(layer));
                    }
                }

                // Layered rendering order
                // Draw SDF primitives first, then batched geometry using LayerManager order
                for (uint16_t layerId : renderOrder) {
                    // === Check layer visibility and render enabled ===
                    if (layerManager) {
                        const auto& layerData = layerManager->Get(layerId);
                        // Skip layers that are disabled or hidden in editor
                        if (!layerData.renderEnabled || !layerData.editorVisible)
                            continue;
                    }

                    int layer = static_cast<int>(layerId);
                    if (layer >= static_cast<int>(buckets.size())) continue;
                    const auto& sourceList = buckets[layer];
                    sortedLayerEntities.assign(sourceList.begin(), sourceList.end());
                    auto& list = sortedLayerEntities;
                    // Sort by ZIndex2D.ZOrder ascending (smaller drawn first). Entities
                    // without ZIndex2D are treated as ZOrder = 0
                    std::sort(list.begin(), list.end(), [&](const ECS::Entity& A, const ECS::Entity& B) {
                        const auto* zA = world.TryGet<Components::ZIndex2D>(A);
                        const auto* zB = world.TryGet<Components::ZIndex2D>(B);
                        const int za = zA ? zA->ZOrder : 0;
                        const int zb = zB ? zB->ZOrder : 0;
                        if (za != zb) return za < zb;
                        // Stable tie-breaker: entity index
                        return A.Index < B.Index;
                    });

                    glm::mat4 layerViewProj = viewProj;

                    // Sub-pass 1: SDF circles on this layer
                    m_sdfCircleShader->use();
                    m_sdfCircleShader->setMat4("uViewProj", viewProj);
                    m_sdfCircleShader->setUniform("uStrokePx", 0.0f);
                    m_sdfCircleShader->setUniform("uUseOverrideColor", 0);   // normal circles use their own color
                    m_renderer->beginFrame();

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (!world.IsActiveInHierarchy(entity)) continue;

                        const auto* sc = world.TryGet<Components::ShapeCircle2D>(entity);
                        if (!sc) continue;
                        const auto* lt = world.TryGet<Components::LocalTransform>(entity);
                        if (!lt) continue;

                        // Transform
                        Vector3D position, scale; Quaternion rotation;
                        GetRenderTransform(world, entity, *lt, position, rotation, scale);

                        // Draw SDF circle
                        DebugDraw2D::Circle(
                            *m_renderer,
                            ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sc->Offset),
                            sc->Radius * ((scale.X + scale.Y) * 0.5f),
                            ToGlm(sc->Color),
                            sc->Filled ? 0.0f : sc->Thickness,
                            /*textureId*/ 0
                        );
                    }

                    m_renderer->endFrame(); // flush SDF for this layer

                    // Sub-pass 2: everything else on this layer
                    m_shader->use();
                    m_shader->setMat4("uViewProj", viewProj);
                    m_shader->setUniform("uPicking", 0);

                    // enable lighting in batch.frag
                    m_shader->setUniform("uLightingEnabled", 1);

                    // bind SSBO + light uniforms (uPointLightCount/uHasDirectional/uDirLight)
                    m_lightManager.Bind(*m_shader);

                    m_renderer->beginFrame();

                    if (m_debugTileMap && m_debugTileset)
                    {
                        // Backward-compatible single debug tilemap path
                        TileMapRenderer tileRenderer;
                        const std::vector<const Tileset*> tilesets = { &m_debugTileset->get() };
                        tileRenderer.Submit(
                            m_debugTileMap->get(),
                            tilesets,
                            *m_renderer,
                            m_debugTileMapOffset
                        );
                    }

                    if (!m_debugTileMaps.empty())
                    {
                        // Render only legacy debug tilemaps that are not owned by an entity
                        // Entity-owned debug tilemaps are submitted in Z-sorted order below
                        TileMapRenderer tileRenderer;

                        // For each debug tilemap entry
                        for (const auto& entry : m_debugTileMaps) {
                            // Skip if source entity is specified but not active/alive (e.g. deleted map)
                            if (!entry.SourceEntity.IsNull()) {
                                continue;
                            }
                            if (entry.Tilesets.empty()) {
                                continue;
                            }

                            // Convert shared_ptr<Tileset> to raw pointer for TileMapRenderer
                            std::vector<const Tileset*> rawTilesets;

                            // Preallocate for efficiency
                            rawTilesets.reserve(entry.Tilesets.size());

                            // Populate raw pointer list
                            for (const auto& ts : entry.Tilesets) {
                                rawTilesets.push_back(ts.get());
                            }

                            // Submit tilemap for rendering
                            tileRenderer.Submit(entry.Map.get(), rawTilesets, *m_renderer, entry.Offset,
                                nullptr);
                        }
                    }

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (!world.IsActiveInHierarchy(entity)) continue;

                        // Skip circles here (already drawn by SDF pass)
                        if (world.TryGet<Components::ShapeCircle2D>(entity)) continue;

                        // Keep tilemap draw order aligned with Z-sorted entity order
                        SubmitRuntimeTileMapEntity(entity);
                        SubmitDebugTileMapEntity(world, entity);

                        // Boid flock entity  flush batch, draw instanced at correct Z
                        if (world.TryGet<Components::BoidFlock>(entity)) {
                            m_renderer->endFrame();

                            if (m_boidSystem && m_boidShader) {
                                m_boidShader->use();
                                m_boidShader->setMat4("uViewProj", viewProj);
                                m_lightManager.Bind(*m_boidShader);
                                m_boidSystem->DrawFlockForEntity(entity.Index, *m_boidShader);
                            }

                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 1);
                            m_lightManager.Bind(*m_shader);
                            m_renderer->beginFrame();
                            continue;
                        }

                        // Particle emitter — flush batch, draw instanced at correct Z
                        if (world.TryGet<Components::ParticleEmitter>(entity)) {
                            m_renderer->endFrame();

                            if (m_particleSystem && m_particleShader) {
                                m_particleShader->use();
                                m_particleShader->setMat4("uViewProj", viewProj);
                                m_lightManager.Bind(*m_particleShader);
                                glEnable(GL_BLEND);
                                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                                m_particleSystem->DrawEmitterForEntity(entity.Index, *m_particleShader, world);
                            }

                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 1);
                            m_lightManager.Bind(*m_shader);
                            m_renderer->beginFrame();
                            continue;
                        }

                        // Fetch transform
                        auto* lt = world.TryGet<Components::LocalTransform>(entity);
                        if (!lt) {
                            continue;
                        }
                        Vector3D position, scale; Quaternion rotation;
                        GetRenderTransform(world, entity, *lt, position, rotation, scale);

                        // Boxes
                        if (world.Has<Components::ShapeBox2D>(entity)) {
                            const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                            const float rotationAngle = 2.0f * std::acos(rotation.W);
                            const bool hasRotation = std::abs(rotationAngle) > 0.01f;

                            if (!hasRotation) {
                                const glm::vec2 halfExtents = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                                const glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                                const glm::vec2 min = center - halfExtents;
                                const glm::vec2 max = center + halfExtents;

                                if (sb.Filled) DebugDraw2D::RectFill(*m_renderer, min, max, ToGlm(sb.Color), 0);
                                else           DebugDraw2D::RectStroke(*m_renderer, min, max, sb.Thickness, ToGlm(sb.Color), 0);
                            }
                            else {
                                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                                transformedCorners.clear(); transformedCorners.reserve(4);
                                const Vector2D he = sb.HalfExtents;
                                const Vector3D corners[4] = {
                                    {-he.X, -he.Y, 0.0f}, { he.X, -he.Y, 0.0f},
                                    { he.X,  he.Y, 0.0f}, {-he.X,  he.Y, 0.0f}
                                };
                                for (auto c : corners) {
                                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                                    transformedCorners.push_back(ToGlm(Vector2D{ hc.X, hc.Y }) + ToGlm(sb.Offset));
                                }
                                if (sb.Filled) {
                                    // Submit polygon geometry
                                    DebugDraw2D::Polygon(*m_renderer, transformedCorners, ToGlm(sb.Color), 0);
                                }
                                else {
                                    for (int i = 0; i < 4; ++i)
                                        // Submit line geometry
                                        DebugDraw2D::Line(*m_renderer, transformedCorners[i], transformedCorners[(i + 1) % 4], sb.Thickness, ToGlm(sb.Color), 0);
                                }
                            }
                        }

                        // Lines
                        if (world.Has<Components::ShapeLine2D>(entity)) {
                            const auto& sl = world.Get<Components::ShapeLine2D>(entity);
                            
                            // Build transformation matrix
                            const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                            
                            // Transform endpoints from local to world space
                            const Vector4D worldA = m * Vector4D{sl.A.X, sl.A.Y, 0.0f, 1.0f};
                            const Vector4D worldB = m * Vector4D{sl.B.X, sl.B.Y, 0.0f, 1.0f};
                            
                            // Submit line geometry
                            DebugDraw2D::Line(*m_renderer,
                                ToGlm(Vector2D{worldA.X, worldA.Y}),
                                ToGlm(Vector2D{worldB.X, worldB.Y}),
                                sl.Thickness, ToGlm(sl.Color), 0);
                        }
                        // Sprites
                        if (world.Has<Components::SpriteRenderer2D>(entity)) {
                            const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                            const float angleZ = std::atan2(
                                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
                            );

                            // Calculate UV coordinates from Tiling and Offset
                            const float u0 = sr.Offset.X;
                            const float v0 = sr.Offset.Y;
                            const float u1 = sr.Offset.X + sr.Tiling.X;
                            const float v1 = sr.Offset.Y + sr.Tiling.Y;

                            // Check if entity has Material2D component for PBR rendering
                            GLuint normalTexId = 0;
                            GLuint mraTexId = 0;
                            float metallic = 0.0f;
                            float smoothness = 0.5f;
                            float aoStrength = 1.0f;
                            float normalStrength = 1.0f;
                            uint32_t flags = 0;

                            if (const auto* mat = world.TryGet<Components::Material2D>(entity)) {
                                normalTexId = mat->NormalTextureId;
                                mraTexId = mat->MRA_TextureId;
                                metallic = mat->Metallic;
                                smoothness = mat->Smoothness;
                                aoStrength = mat->AOStrength;
                                normalStrength = mat->NormalStrength;
                                flags = mat->Flags;
                                if (normalTexId == 0) {
                                    normalTexId = sr.NormalTextureId;
                                }
                            }

                            m_renderer->submitSprite({
                                ToGlm(Vector2D{position.X, position.Y}),
                                ToGlm(Vector2D{scale.X, scale.Y}),
                                {u0, v0, u1, v1},
                                ToGlm(sr.Color),
                                sr.TextureId,
                                angleZ,
                                1.0f,
                                sr.EmissiveTextureId,
                                sr.EmissiveStrength,
                                sr.Width,           // texture width
                                sr.Height,          // texture height
                                normalTexId,        // Material2D: normal map
                                mraTexId,           // Material2D: MRA map
                                metallic,           // Material2D: metallic value
                                smoothness,         // Material2D: smoothness value
                                aoStrength,         // Material2D: AO strength
                                normalStrength,     // Material2D: normal strength
                                flags,
                                sr.TextureFilter
                                });
                        }
                    }

                    m_renderer->endFrame(); // flush non-SDF for this layer
                }

                // Unbind the current render target
                Framebuffer::Unbind();
            });

        // Object Picking Pass
        m_renderGraph->AddPass("Picking", {}, {},
            [this, &world, &viewProj, &buckets, &win](ResourceAccessor& res)
            {
                // Note: Do NOT skip picking simply because an external camera
                // is set. Editor viewports set an external camera to preview
                // their view; picking should still run if a mouse click or
                // a pending async pick request exists. Earlier logic that
                // unconditionally skipped when m_activeCamera was present
                // prevented the editor from picking. The pass below will
                // early-return when there's no interactive click and no
                // pending request
                (void)res;
                // Allow the picking pass to run if there is a pending async request or mouse click
                if (!Input::IsMousePressed(MOUSE_LEFT) && m_pendingPickRequests.empty() && !m_currentPickRequest.has_value() && !m_inFlightPick.has_value()) return;

                // Dequeue the next pending request if current one is done
                if (!m_currentPickRequest.has_value() && !m_pendingPickRequests.empty()) {
                    m_currentPickRequest = m_pendingPickRequests.front();
                    m_pendingPickRequests.pop();
                }

                // ============================================================
                // GET VIEWPORT BOUNDS
                // ============================================================
                // By default the picking FBO covers the full window. If an
                // async pick request was submitted by the editor for a
                // sub-region viewport, use that viewport's rect for mapping
                // screen coordinates into FBO texels
                glm::vec2 viewportMin(0, 0);
                glm::vec2 viewportSize = glm::vec2(win->GetWidth(), win->GetHeight());

                glm::dvec2 mousePos;
                // Return mouse position
                Input::GetMousePosition(mousePos.x, mousePos.y);

                // If there is a current async pick request being processed, 
                // use its viewport rectangle for coordinate mapping
                bool usingCurrentRequest = false;
                if (m_currentPickRequest.has_value()) {
                    viewportMin = m_currentPickRequest->ViewportPos;
                    viewportSize = m_currentPickRequest->ViewportSize;
                    usingCurrentRequest = true;
                }

                // ============================================================
                // RESIZE PICKING FBO IF NEEDED
                // ============================================================
                int vpWidth = static_cast<int>(viewportSize.x);
                int vpHeight = static_cast<int>(viewportSize.y);

                // Check if picking FBO needs resize
                if (m_pickingFBO.Width() != vpWidth || m_pickingFBO.Height() != vpHeight) {
                    LOG_DEBUG("[PICKING] Resizing picking FBO: " << vpWidth << "x" << vpHeight);
                    m_pickingFBO.Resize(vpWidth, vpHeight, false, false);
                }

                // Resolve previous async pick readback
                // If last frame submitted a pick request, its PBO now holds the result
                // Decode that pixel and store requestId to entityId for TryGetPickResult polling
                if (m_inFlightPick.has_value()) {
                    const int idx = m_inFlightPick->PBOIndex;
                    if (idx >= 0 && idx < 2) {
                        m_pbos[idx].Bind(GL_PIXEL_PACK_BUFFER);
                        void* mapped = m_pbos[idx].Map(GL_READ_ONLY);
                        if (mapped) {
                            uint8_t* bytes = static_cast<uint8_t*>(mapped);
                            uint32_t encoded = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
                            uint32_t pickedEntity = (encoded == 0) ? INVALID_ENTITY_ID : (encoded - 1);
                            m_completedPickResults[m_inFlightPick->RequestId] = pickedEntity;
                            LOG_DEBUG("[PICKING] In-flight PBO " << idx << " decoded request " << m_inFlightPick->RequestId << " -> entity " << pickedEntity);
                            m_pbos[idx].Unmap();
                        }
                        else {
                            LOG_DEBUG("[PICKING] Warning: failed to map PBO " << idx << " for readback");
                        }
                        m_pbos[idx].Unbind(GL_PIXEL_PACK_BUFFER);
                    }
                    m_inFlightPick.reset();
                }

                m_pickingFBO.BindAndClear(0, 0, 0, 1);

                // Set viewport to match FBO size
                glViewport(0, 0, vpWidth, vpHeight);

                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
                if (blendWasEnabled) glDisable(GL_BLEND);

                // ============================================================
                // Pass 1: Render circles with SDF shader
                // ============================================================
                m_sdfCircleShader->use();
                m_sdfCircleShader->setMat4("uViewProj", viewProj);
                m_sdfCircleShader->setUniform("uPicking", 1);
                m_shader->setUniform("uLightingEnabled", 0);
                m_renderer->beginFrame();

                for (int layer = 0; layer <= static_cast<int>(buckets.size()) - 1; ++layer) {
                    const auto& list = buckets[layer];

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (!world.IsActiveInHierarchy(entity)) continue;

                        // Only render circles in this pass
                        if (!world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Encode entity ID as RGB
                        uint32_t id = entity.Index + 1;
                        glm::vec4 idColor(
                            ((id >> 0) & 0xFF) / 255.0f,
                            ((id >> 8) & 0xFF) / 255.0f,
                            ((id >> 16) & 0xFF) / 255.0f,
                            1.0f
                        );

                        // Get transform
                        const auto& lt = world.Get<Components::LocalTransform>(entity);
                        Vector3D position, scale;
                        Quaternion rotation;
                        GetRenderTransform(world, entity, lt, position, rotation, scale);

                        const auto& sc = world.Get<Components::ShapeCircle2D>(entity);
                        // Submit circle geometry
                        DebugDraw2D::Circle(
                            *m_renderer,
                            ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sc.Offset),
                            sc.Radius * ((scale.X + scale.Y) * 0.5f),
                            idColor,
                            0.0f,
                            0
                        );
                    }
                }

                m_renderer->endFrame();

                // ============================================================
                // Pass 2: Render boxes and sprites with batch shader
                // ============================================================
                m_shader->use();
                m_shader->setMat4("uViewProj", viewProj);
                m_shader->setUniform("uPicking", 1);
                m_shader->setUniform("uLightingEnabled", 0);

                m_renderer->beginFrame();

                for (int layer = 0; layer <= static_cast<int>(buckets.size()) - 1; ++layer) {
                    const auto& list = buckets[layer];

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (!world.IsActiveInHierarchy(entity)) continue;

                        // Skip circles (already rendered above)
                        if (world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Encode entity ID as RGB
                        uint32_t id = entity.Index + 1;
                        glm::vec4 idColor(
                            ((id >> 0) & 0xFF) / 255.0f,
                            ((id >> 8) & 0xFF) / 255.0f,
                            ((id >> 16) & 0xFF) / 255.0f,
                            1.0f
                        );

                        // Get transform
                        const auto& lt = world.Get<Components::LocalTransform>(entity);
                        Vector3D position, scale;
                        Quaternion rotation;
                        GetRenderTransform(world, entity, lt, position, rotation, scale);

                        // Render BOXES with ID color
                        if (world.Has<Components::ShapeBox2D>(entity)) {
                            const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                            const glm::vec2 halfExtents = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                            const glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                            const glm::vec2 min = center - halfExtents;
                            const glm::vec2 max = center + halfExtents;

                            // Use -1 so the shader treats this as a solid-color shape (no texture sampling)
                            // In the picking shader, texIndex >= 0 samples a texture and may alpha-discard
                            // Negative indices skip sampling and always write the ID color
                            DebugDraw2D::RectFill(*m_renderer, min, max, idColor, static_cast<GLuint>(-1));
                        }

                        // Render LINES with ID color
                        if (world.Has<Components::ShapeLine2D>(entity)) {
                            const auto& sl = world.Get<Components::ShapeLine2D>(entity);
                            const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                            const Vector4D worldA = m * Vector4D{ sl.A.X, sl.A.Y, 0.0f, 1.0f };
                            const Vector4D worldB = m * Vector4D{ sl.B.X, sl.B.Y, 0.0f, 1.0f };
                            DebugDraw2D::Line(
                                *m_renderer,
                                ToGlm(Vector2D{ worldA.X, worldA.Y }),
                                ToGlm(Vector2D{ worldB.X, worldB.Y }),
                                sl.Thickness,
                                idColor,
                                static_cast<GLuint>(-1)
                            );
                        }

                        // Render SPRITES with ID color
                        if (world.Has<Components::SpriteRenderer2D>(entity)) {
                            const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                            const float angleZ = std::atan2(
                                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
                            );
                            m_renderer->submitSprite({
                                ToGlm(Vector2D{position.X, position.Y}),
                                ToGlm(Vector2D{scale.X, scale.Y}),
                                {sr.Offset.X, sr.Offset.Y, sr.Offset.X + sr.Tiling.X, sr.Offset.Y + sr.Tiling.Y},
                                idColor,
                                static_cast<GLuint>(-1), // Solid ID quad for robust picking (no alpha discard).
                                angleZ,
                                1.0f,
                                0,      // emissiveTextureId (no emissive in picking pass)
                                0.0f,   // emissiveStrength (no emissive in picking pass)
                                0,      // texture width
                                0,      // texture height
                                0,      // normalTextureId
                                0,      // mraTextureId
                                0.0f,   // metallic
                                0.5f,   // smoothness
                                1.0f,   // aoStrength
                                1.0f,   // normalStrength
                                0,      // materialFlags
                                sr.TextureFilter
                                });
                        }
                    }
                }

                m_renderer->endFrame();

                // ============================================================
                // READ PIXEL (now in FBO-local coordinates)
                // ============================================================
                // Determine which screen coordinates to sample. If a current
                // pick request is being processed, use its coordinates
                // Otherwise use the current mouse position (interactive click)
                glm::vec2 sampleScreenPos;
                bool usingCurrentPickRequest = false;
                if (m_currentPickRequest.has_value()) {
                    sampleScreenPos = glm::vec2(m_currentPickRequest->ScreenX, m_currentPickRequest->ScreenY);
                    usingCurrentPickRequest = true;
                }
                else {
                    sampleScreenPos = glm::vec2(mousePos.x, mousePos.y);
                }

                // Convert sampleScreenPos into viewport-local coordinates
                glm::vec2 localPos = sampleScreenPos - viewportMin;

                // Map viewport-local coordinates to FBO pixel coordinates
                const int fboWidth = m_pickingFBO.Width();
                const int fboHeight = m_pickingFBO.Height();

                int readX = 0;
                int readY = 0;
                vpWidth = static_cast<int>(viewportSize.x);
                vpHeight = static_cast<int>(viewportSize.y);

                if (fboWidth != vpWidth || fboHeight != vpHeight) {
                    const float sx = static_cast<float>(fboWidth) / static_cast<float>(vpWidth);
                    const float sy = static_cast<float>(fboHeight) / static_cast<float>(vpHeight);
                    readX = glm::clamp(static_cast<int>(localPos.x * sx), 0, fboWidth - 1);
                    // Flip Y: viewport local origin is top-left for screen coords
                    readY = glm::clamp(static_cast<int>((vpHeight - localPos.y - 1.0f) * sy), 0, fboHeight - 1);
                }
                else {
                    // 1:1 mapping
                    readX = glm::clamp(static_cast<int>(localPos.x), 0, fboWidth - 1);
                    readY = glm::clamp(static_cast<int>(vpHeight - localPos.y - 1.0f), 0, fboHeight - 1);
                }

                LOG_DEBUG("[PICKING] FBO size: " << fboWidth << "x" << fboHeight);
                LOG_DEBUG("[PICKING] Reading pixel: (" << readX << ", " << readY << ")");
                if (usingCurrentPickRequest && m_currentPickRequest.has_value()) {
                    LOG_DEBUG("[PICKING] Servicing async request " << m_currentPickRequest->RequestId);
                }

                // Frame N: Write to current PBO (async transfer starts)
                m_pbos[m_currentPBO].Bind(GL_PIXEL_PACK_BUFFER);
                glReadPixels(readX, readY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                m_pbos[m_currentPBO].Unbind(GL_PIXEL_PACK_BUFFER);

                // If this read corresponds to a current async pick request,
                // mark it as in-flight and associate it with the current PBO
                // so the result can be consumed on the next frame
                if (usingCurrentPickRequest) {
                    m_inFlightPick = InFlightPick{ m_currentPickRequest->RequestId, m_currentPBO };
                    m_currentPickRequest.reset();
                }

                // Swap PBOs for the next frame
                m_currentPBO = 1 - m_currentPBO;


                // Restore blending state
                if (blendWasEnabled) glEnable(GL_BLEND);

                // Unbind the current render target
                Framebuffer::Unbind();

                // Restore viewport to full window
                glViewport(0, 0, win->GetWidth(), win->GetHeight());
            });


        m_renderGraph->AddPass("WorldGUI", { "HDR" }, { "HDR" },
            [this, &viewProj](ResourceAccessor& res)
            {
                auto* hdrFbo = res.GetFramebuffer("HDR");
                if (!hdrFbo) return;

                hdrFbo->Bind();
                glViewport(0, 0, hdrFbo->Width(), hdrFbo->Height());
                RenderWorldGUI(viewProj);
                Framebuffer::Unbind();

                m_worldGuiPanelQueue.clear();
                m_worldGuiImageQueue.clear();
                m_worldGuiTextQueue.clear();
            });

        m_renderGraph->AddPass("BloomExtract", { "HDR" }, { "BloomExtract" },
            [this](ResourceAccessor& res)
            {
                auto* hdrFbo = res.GetFramebuffer("HDR");
                    auto* extractFbo = res.GetFramebuffer("BloomExtract");
                    if (!hdrFbo || !extractFbo) return;

                    extractFbo->BindAndClear(0, 0, 0, 1);
                    m_bloomExtractShader->use();
                    m_bloomExtractShader->setUniform("uThreshold", 1.1f);   // brightness threshold
                    m_bloomExtractShader->setUniform("uScene", 0);
                    hdrFbo->BindColorTexture(0);                            // texture unit 0 in shader
                    m_renderer->drawFullscreenQuad();
                    // Unbind the current render target
                    Framebuffer::Unbind();
                });

        m_renderGraph->AddPass("BloomBlurH", { "BloomExtract" }, { "BloomBlur" },
            [this, &bloomRadiusTexels](ResourceAccessor& res)
            {
                auto* src = res.GetFramebuffer("BloomExtract");
                auto* dst = res.GetFramebuffer("BloomBlur");
                if (!src || !dst) return;

                dst->BindAndClear(0, 0, 0, 1);
                m_bloomBlurShader->use();
                m_bloomBlurShader->setUniform("uHorizontal", 1);
                m_bloomBlurShader->setUniform("uImage", 0);
                m_bloomBlurShader->setUniform("uRadius", bloomRadiusTexels);
                m_bloomBlurShader->setUniform("uSamples", std::max(12, static_cast<int>(bloomRadiusTexels * 0.6f)));     // Increase uSamples proportionally to uRadius
                m_bloomBlurShader->setUniform("uFalloff", 0.15f);  // LESS FALLOFF
                src->BindColorTexture(0);
                m_renderer->drawFullscreenQuad();
                // Unbind the current render target
                Framebuffer::Unbind();
            });

        m_renderGraph->AddPass("BloomBlurV", { "BloomBlur" }, { "BloomExtract" },
            [this, &bloomRadiusTexels](ResourceAccessor& res)
            {
                auto* src = res.GetFramebuffer("BloomBlur");
                auto* dst = res.GetFramebuffer("BloomExtract");
                if (!src || !dst) return;

                dst->BindAndClear(0, 0, 0, 1);
                m_bloomBlurShader->use();
                m_bloomBlurShader->setUniform("uHorizontal", 0);
                m_bloomBlurShader->setUniform("uImage", 0);
                m_bloomBlurShader->setUniform("uRadius", bloomRadiusTexels);
                m_bloomBlurShader->setUniform("uSamples", std::max(12, static_cast<int>(bloomRadiusTexels * 0.6f)));     // Increase uSamples proportionally to uRadius
                m_bloomBlurShader->setUniform("uFalloff", 0.15f);  // LESS FALLOFF

                src->BindColorTexture(0);
                m_renderer->drawFullscreenQuad();
                // Unbind the current render target
                Framebuffer::Unbind();
            });

        // ToneMap pass -> writes final color to LDR texture
        m_renderGraph->AddPass("ToneMap", { "HDR", "BloomExtract" }, { "LDR" },
            [this](ResourceAccessor& res)
            {
                auto* hdr = res.GetFramebuffer("HDR");
                auto* bloom = res.GetFramebuffer("BloomExtract");
                auto* ldr = res.GetFramebuffer("LDR");
                if (!hdr || !bloom || !ldr) return;

                ldr->BindAndClear(0, 0, 0, 1);

                m_bloomCombineShader->use();
                m_bloomCombineShader->setUniform("uScene", 0);
                m_bloomCombineShader->setUniform("uBloomBlur", 1);
                m_bloomCombineShader->setUniform("uExposure", 1.3f);
                m_bloomCombineShader->setUniform("uBloomStrength", 5.2f);
                m_bloomCombineShader->setUniform("uGamma", 1.5f);

                hdr->BindColorTexture(0, 0);
                bloom->BindColorTexture(0, 1);

                m_renderer->drawFullscreenQuad();
                // Unbind the current render target
                Framebuffer::Unbind();
            });

        // Wireframe Pass - Render debug/editor wireframes on top of tone-mapped scene
        m_renderGraph->AddPass("Wireframe", { "LDR" }, { "LDR" },
            [this, &viewProj](ResourceAccessor& res)
            {
                // Skip if nothing is queued for overlays or wireframes
                if (m_wireframeQueue.empty() && m_overlayQuadQueue.empty()) return;

                auto* ldr = res.GetFramebuffer("LDR");
                if (!ldr) return;

                // Bind LDR framebuffer for rendering on top of tone-mapped content
                ldr->Bind();

                // Enable blending for wireframes
                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Process all queued overlay quads (textured previews, etc.)
                if (!m_overlayQuadQueue.empty()) {
                    if (m_shader) {
                        m_shader->use();
                        m_shader->setMat4("uViewProj", viewProj);
                        m_shader->setUniform("uPicking", 0);
                        m_shader->setUniform("uLightingEnabled", 0);
                    }
                    m_renderer->beginFrame();
                    for (const auto& quad : m_overlayQuadQueue) {
                        m_renderer->submitQuad(
                            quad.center,
                            quad.size,
                            quad.textureId,
                            quad.uvRect,
                            quad.color,
                            quad.rotation,
                            1.0f,
                            0
                        );
                    }
                    m_renderer->endFrame();
                }

                // Process all queued wireframe submissions
                for (const auto& sub : m_wireframeQueue) {
                    switch (sub.type) {
                    case WireframeSubmission::Type::Circle: {
                        // Circles need SDF shader
                        m_sdfCircleShader->use();
                        m_sdfCircleShader->setMat4("uViewProj", viewProj);
                        m_sdfCircleShader->setUniform("uPicking", 0);
                        m_renderer->beginFrame();

                        // Submit circle geometry
                        DebugDraw2D::Circle(*m_renderer,
                            sub.center,
                            sub.radius,
                            sub.color,
                            sub.filled ? 0.0f : sub.thickness,
                            0
                        );

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Polygon: {
                        // Use batch shader for line-based shapes
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (sub.filled && sub.vertices.size() >= 3) {
                            // Submit polygon geometry
                            DebugDraw2D::Polygon(*m_renderer, sub.vertices, sub.color, 0);
                        }
                        else if (sub.vertices.size() >= 2) {
                            for (size_t i = 0; i < sub.vertices.size(); ++i) {
                                size_t next = sub.closed ? (i + 1) % sub.vertices.size() : i + 1;
                                if (next < sub.vertices.size()) {
                                    // Submit line geometry
                                    DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[next],
                                        sub.thickness, sub.color, 0);
                                }
                            }
                        }

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Line: {
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (sub.vertices.size() == 2) {
                            // Submit line geometry
                            DebugDraw2D::Line(*m_renderer, sub.vertices[0], sub.vertices[1],
                                sub.thickness, sub.color, 0);
                        }

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Mesh: {
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (!sub.indices.empty()) {
                            // Draw using indices
                            for (size_t i = 0; i < sub.indices.size(); i += 2) {
                                if (i + 1 < sub.indices.size()) {
                                    uint32_t idx0 = sub.indices[i];
                                    uint32_t idx1 = sub.indices[i + 1];
                                    if (idx0 < sub.vertices.size() && idx1 < sub.vertices.size()) {
                                        // Submit line geometry
                                        DebugDraw2D::Line(*m_renderer, sub.vertices[idx0], sub.vertices[idx1],
                                            sub.thickness, sub.color, 0);
                                    }
                                }
                            }
                        }
                        else {
                            // Draw as sequence of lines
                            for (size_t i = 0; i + 1 < sub.vertices.size(); i += 2) {
                                DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[i + 1],
                                    sub.thickness, sub.color, 0);
                            }
                        }

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Quad: {
                        // Switch to batch shader for non-circle shapes
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (sub.vertices.size() == 4) {
                            const auto& min = sub.vertices[0];
                            const auto& max = sub.vertices[2];
                            if (sub.filled) {
                                // Submit filled rectangle geometry
                                DebugDraw2D::RectFill(*m_renderer, min, max, sub.color, 0);
                            }
                            else {
                                // Submit rectangle outline geometry
                                DebugDraw2D::RectStroke(*m_renderer, min, max, sub.thickness, sub.color, 0);
                            }
                        }
                        m_renderer->endFrame();
                        break;
                    }
                    }
                }

                m_renderer->endFrame();

                // Clear overlay/wireframe queues for next frame
                m_overlayQuadQueue.clear();
                m_wireframeQueue.clear();

                // Restore blend state
                if (!blendWasEnabled) glDisable(GL_BLEND);

                // Unbind the current render target
                Framebuffer::Unbind();
            });

        // GUI Pass - Render GUI panels on top of scene
        m_renderGraph->AddPass("GUI", { "LDR" }, { "LDR" },
            [this](ResourceAccessor& res)
            {
                // Skip if no GUI elements queued
                if (m_guiPanelQueue.empty() && m_guiTextQueue.empty() && m_guiImageQueue.empty()) return;

                auto* ldr = res.GetFramebuffer("LDR");
                if (!ldr) return;

                // Setup orthographic projection for screen-space rendering
                const float width = static_cast<float>(ldr->Width());
                const float height = static_cast<float>(ldr->Height());

                ldr->Bind();
                glViewport(0, 0, ldr->Width(), ldr->Height());

                // Enable blending for GUI rendering
                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND); // preserve blend state for GUI pass
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Orthographic projection for GUI panels/images (top-left origin)
                glm::mat4 screenOrtho = glm::ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);

                // Setup scissor test based on GUI viewport
                bool scissorEnabled = false;
                GUIViewport viewport = m_guiViewport;
                if (!viewport.Active || viewport.Size.X <= 0.0f || viewport.Size.Y <= 0.0f) {
                    viewport.Origin = { 0.0f, 0.0f };
                    viewport.Size = { width, height };
                }

                // Enable scissor test if viewport size is valid
                if (viewport.Size.X > 0.0f && viewport.Size.Y > 0.0f) {
                    glEnable(GL_SCISSOR_TEST);
                    scissorEnabled = true;
                    const int scissorX = static_cast<int>(std::round(viewport.Origin.X));
                    const int scissorY = static_cast<int>(std::round(height - (viewport.Origin.Y + viewport.Size.Y)));
                    const int scissorW = static_cast<int>(std::round(viewport.Size.X));
                    const int scissorH = static_cast<int>(std::round(viewport.Size.Y));
                    glScissor(scissorX, scissorY, scissorW, scissorH);
                }

                Renderer* guiRenderer = m_guiRenderer ? m_guiRenderer.get() : m_renderer.get();
                if (!guiRenderer) {
                    return;
                }

                // Render GUI panels (solid quads with optional corner radius)
                if (!m_guiPanelQueue.empty()) {
                    Shader* guiShader = m_guiShader ? m_guiShader.get() : m_shader.get();
                    if (guiShader) {
                        guiShader->use();
                        guiShader->setMat4("uViewProj", screenOrtho);
                        guiShader->setUniform("uGamma", 1.5f);
                    }
                    guiRenderer->beginFrame();
                    for (const auto& panel : m_guiPanelQueue) { // Render each panel
                        const glm::vec2 center(panel.position.X + panel.size.X * 0.5f,
                                               panel.position.Y + panel.size.Y * 0.5f);
                        const glm::vec2 size(panel.size.X, panel.size.Y);
                        const glm::vec4 color(panel.color.R, panel.color.G, panel.color.B, panel.color.A);
                        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
                        GLuint textureId = 0;
                        guiRenderer->submitQuad(center, size, textureId, uvRect, color, 0.0f, 1.0f, 0, 0u, 0.0f);
                    }
                    guiRenderer->endFrame();
                }

                // Render GUI images/icons (textured quads)
                if (!m_guiImageQueue.empty()) {
                    Shader* guiShader = m_guiShader ? m_guiShader.get() : m_shader.get();
                    if (guiShader) {
                        guiShader->use();
                        guiShader->setMat4("uViewProj", screenOrtho);
                        guiShader->setUniform("uGamma", 1.5f);
                    }
                    guiRenderer->beginFrame();
                    for (const auto& image : m_guiImageQueue) {
                        const glm::vec2 center(image.position.X + image.size.X * 0.5f,
                                               image.position.Y + image.size.Y * 0.5f);
                        const glm::vec2 size(image.size.X, image.size.Y);
                        const glm::vec4 color(image.color.R, image.color.G, image.color.B, image.color.A);
                        // GUI projection uses Y-down; flip V to keep textures upright
                        const glm::vec4 uvRect(image.uvRect.X, image.uvRect.W, image.uvRect.Z, image.uvRect.Y);
                        const GLuint textureId = image.textureId;
                        guiRenderer->submitQuad(center, size, textureId, uvRect, color, 0.0f, 1.0f, 0, 0u, 0.0f,
                            0, 0, 0.0f, 0.5f, 1.0f, 1.0f, 0, image.textureFilter);
                    }
                    guiRenderer->endFrame();
                }

                // Render GUI text (SDF font rendering in screen space)
                if (!m_guiTextQueue.empty()) {
                    const glm::mat4 textOrtho = glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
                    if (m_textShader) {
                        m_textShader->use();
                        m_textShader->setMat4("uProjection", textOrtho);
                    }
                    guiRenderer->beginFrame();
                    for (const auto& text : m_guiTextQueue) { // Render each text element
                        if (text.text.empty()) {
                            continue;
                        }

                        // Load font (fallback to default if path is empty)
                        const std::string fontPath = text.fontPath.empty()
                            ? std::string("assets/fonts/Roboto/static/Roboto-Regular.ttf")
                            : text.fontPath;
                        const int pixelSize = std::max(1, static_cast<int>(std::round(text.pixelSize)));
                        auto font = RM.GetFont(fontPath, pixelSize);
                        if (!font) {
                            continue;
                        }

                        // Calculate text position (flip Y for GUI space, anchor from top)
                        const float scale = text.pixelSize / static_cast<float>(font->getPixelSize());
                        const float ascent = font->getAscent() * scale;
                        const glm::vec2 textPos(text.position.X, height - text.position.Y - ascent);
                        const glm::vec4 color(text.color.R, text.color.G, text.color.B, text.color.A);
                        guiRenderer->submitText(*font, text.text, textPos, color, text.pixelSize);
                    }
                    guiRenderer->endFrame();
                }

                // Disable scissor test if it was enabled
                if (scissorEnabled) {
                    glDisable(GL_SCISSOR_TEST);
                }

                // Clear GUI queues for next frame
                m_guiPanelQueue.clear();
                m_guiImageQueue.clear();
                m_guiTextQueue.clear();
                m_worldGuiPanelQueue.clear();
                m_worldGuiImageQueue.clear();
                m_worldGuiTextQueue.clear();

                if (!blendWasEnabled) glDisable(GL_BLEND);
                // Unbind the current render target
                Framebuffer::Unbind();
            });

        // Blit LDR to backbuffer
        m_renderGraph->AddPass("Composite", { "LDR" }, { "Backbuffer" },
            [this, &win](ResourceAccessor& res)
            {
                auto* ldr = res.GetFramebuffer("LDR");
                if (!ldr) return;

                // Bind default
                Framebuffer::BindDefault();
                glViewport(0, 0, win->GetWidth(), win->GetHeight());

                // Use a simple blit shader, NOT bloomCombine
                m_blitShader->use();
                m_blitShader->setUniform("uTex", 0);
                ldr->BindColorTexture(0, 0);

                m_renderer->drawFullscreenQuad();
            });

        // ============================================================
        // EXECUTE RENDER GRAPH
        // ============================================================
        m_renderGraph->Execute();

        // Performance logging
        if (TimeSystem::Instance().GetFrameCount() % 120 == 0)
        {
            static int previousFlushTotal = 0;
            int currentTotal = GetFlushCount();
            int flushes = currentTotal - previousFlushTotal;
            previousFlushTotal = currentTotal;

            std::stringstream ss;
            if (flushes > 10)
                ss << " Too many flushes! Likely texture switches or buffer overflows...";
            else if (flushes == 1)
                ss << " Single batch, bottleneck is CPU-side or GPU fillrate";

            LOG_DEBUG("Flushes this frame: " << flushes << ss.str() << " | " << "FPS: " <<  static_cast<int>(1.0f / TimeSystem::Instance().GetDeltaTime()));
        }
    }

    // Tear down system state
    void RendererSystem::OnDestroy(World& world) {
        (void)world;
        // Cleanup rendering resources
        m_renderer.reset();
        m_guiRenderer.reset();
        m_renderGraph.reset();
        m_shader.reset();
        m_guiShader.reset();
        m_textShader.reset();
        m_sdfCircleShader.reset();
        m_blitShader.reset();
        m_bloomBlurShader.reset();
        m_bloomExtractShader.reset();
        m_bloomCombineShader.reset();
        m_pickingFBO.Destroy();
        m_runtimeTileMaps.clear();
        g_rendererSystemInstance = nullptr;
        m_lightManager.Shutdown();
    }

    // ====================================================================
    // Viewport Management Implementation
    // ====================================================================

    // Create or replace a named viewport and allocate all pass framebuffers for it
    void RendererSystem::AddViewport(const std::string& name, Engine::Camera* camera, int w, int h) {
        // Check if viewport already exists
        for (auto& vp : m_viewports) {
            if (vp.Name == name) {
                // Keep the viewport slot but swap camera and resize render targets in place
                vp.Camera = camera;
                ResizeViewport(name, w, h);
                return;
            }
        }

        Viewport vp;
        vp.Name = name;
        vp.Camera = camera;
        vp.Size = { std::max(1, w), std::max(1, h) };
        vp.Active = true;

        // Create per-viewport FBOs
        // HDR stores lit scene data in high precision before post processing
        vp.HDR = std::make_unique<Framebuffer>();
        vp.HDR->Create(vp.Size.x, vp.Size.y, true, true, 1);

        // LDR stores final tone-mapped output used by editor panels and presentation
        vp.LDR = std::make_unique<Framebuffer>();
        vp.LDR->Create(vp.Size.x, vp.Size.y, false, false, 1);

        // Bloom passes run at half resolution to reduce fill cost while preserving soft glow
        vp.BloomExtract = std::make_unique<Framebuffer>();
        vp.BloomExtract->Create(vp.Size.x / 2, vp.Size.y / 2, true, false, 1);

        vp.BloomBlur = std::make_unique<Framebuffer>();
        vp.BloomBlur->Create(vp.Size.x / 2, vp.Size.y / 2, true, false, 1);

        vp.PickingFBO = std::make_unique<Framebuffer>();
        vp.PickingFBO->Create(vp.Size.x, vp.Size.y, false, false, 1);

        m_viewports.push_back(std::move(vp));

        LOG_DEBUG("[Viewport] Created '" << name << "' (" << w << "x" << h << ")");
    }

    // Remove viewport
    void RendererSystem::RemoveViewport(const std::string& name) {
        m_viewports.erase(
            // Remove items that no longer match filters
            std::remove_if(m_viewports.begin(), m_viewports.end(),
                [&](const Viewport& vp) { return vp.Name == name; }),
            m_viewports.end());

        LOG_DEBUG("[Viewport] Removed '" << name << "'");
    }

    // Resize viewport
    void RendererSystem::ResizeViewport(const std::string& name, int w, int h) {
        Viewport* vp = GetViewport(name);
        if (!vp) return;

        w = std::max(1, w);
        h = std::max(1, h);

        if (vp->Size.x == w && vp->Size.y == h) return;

        vp->Size = { w, h };

        vp->HDR->Resize(w, h, true, true);
        vp->LDR->Resize(w, h, false, false);
        vp->BloomExtract->Resize(w / 2, h / 2, true, false);
        vp->BloomBlur->Resize(w / 2, h / 2, true, false);
        vp->PickingFBO->Resize(w, h, false, false);

        LOG_DEBUG("[Viewport] Resized '" << name << "' to " << w << "x" << h);
    }

    // Set viewport camera
    void RendererSystem::SetViewportCamera(const std::string& name, Engine::Camera* camera) {
        if (Viewport* vp = GetViewport(name))
            vp->Camera = camera;
    }

    /**
     * @brief Toggle bloom extraction/blur execution for a named viewport.
     * @param name Viewport name key.
     * @param enabled True to execute bloom passes; false to bypass bloom.
     * @return void
     * @complexity O(V) where V is the number of registered viewports.
     */
    void RendererSystem::SetViewportBloomEnabled(const std::string& name, const bool enabled) {
        if (Viewport* vp = GetViewport(name)) {
            vp->BloomEnabled = enabled;
        }
    }

    // Return viewport
    RendererSystem::Viewport* RendererSystem::GetViewport(const std::string& name) {
        for (auto& vp : m_viewports)
            if (vp.Name == name) return &vp;
        return nullptr;
    }

    // Return viewport texture
    GLuint RendererSystem::GetViewportTexture(const std::string& name) const {
        for (const auto& vp : m_viewports)
            if (vp.Name == name && vp.LDR)
                return vp.LDR->GetColorTexture(0);
        return 0;
    }

    // ====================================================================
    // Extracted Render Helpers
    // ====================================================================

    // Refresh or load tilemaps for all entities that have a TileMapComponent
    void RendererSystem::RefreshRuntimeTileMaps(World& world) {
        // Only refresh tilemaps in Game mode
        // In Editor mode, tilemaps are managed by the editor and should not be overridden by runtime loading logic
        if (Engine::CORE->GetMode() != Engine::EngineMode::Game) {
            return;
        }

        // Track which entities we've seen with TileMapComponents to identify removed ones
        std::unordered_set<EntityId> seen;
        world.Each<ECS::Components::TileMapComponent>([this, &seen, &world](const ECS::Entity entity, ECS::Components::TileMapComponent& comp) {
            // Mark this entity as seen
            seen.insert(entity.Index);

            // Find or create runtime entry for this entity
            RuntimeTileMapEntry& entry = m_runtimeTileMaps[entity.Index];
            const bool generationChanged = (entry.Generation != entity.Generation);

            // If the generation has changed, it means the entity was destroyed and possibly recreated
            // In that case, we should reset the entry to avoid carrying over stale data from a previous entity with the same index
            if (generationChanged) {
                entry = RuntimeTileMapEntry{};
            }
            // Update generation to current entity generation
            entry.Generation = entity.Generation;

            // Resolve paths
            std::string mapPath = ECS::StringTable::Resolve(comp.TileMapPath);
            std::string legacyTilesetPath = ECS::StringTable::Resolve(comp.TilesetTexturePath);
            mapPath = ResolveProjectPathForLoad(mapPath);
            legacyTilesetPath = ResolveProjectPathForLoad(legacyTilesetPath);
            const bool legacyTilesetExists = (!legacyTilesetPath.empty() && std::filesystem::exists(legacyTilesetPath));

            // Determine if we need to reload the map based on changes to the map path, tile world size or default dimensions
            const bool mapNeedsReload = generationChanged || entry.MapPath != mapPath || entry.TileWorldSize != comp.TileWorldSize ||
                entry.DefaultWidth != comp.DefaultWidth || entry.DefaultHeight != comp.DefaultHeight;

            // Reload map if needed
            if (mapNeedsReload) {
                entry.Map.reset();
                entry.Tilesets.clear();
                entry.TilesetPaths.clear();
                entry.MapPath = mapPath;
                entry.TileWorldSize = comp.TileWorldSize;
                entry.DefaultWidth = comp.DefaultWidth;
                entry.DefaultHeight = comp.DefaultHeight;

                // First try loading the map from the specified path
                // If that fails and a legacy tileset path is provided, we'll attempt to load the map without tileset references, relying on fallback logic to find the tileset
                if (!mapPath.empty() && std::filesystem::exists(mapPath)) {
                    entry.Map = std::make_shared<TileMap>(comp.TileWorldSize);
                    // If loading fails
                    // Reset the map pointer to ensure we don't keep an invalid map
                    if (!entry.Map->LoadMap(mapPath)) {
                        LOG_WARNING("[TileMap] (Runtime) Failed to load tilemap: " << mapPath);
                        entry.Map.reset();
                    }
                } 
                // If the map path is specified but the file doesn't exist
                else if (!mapPath.empty()) {
                    LOG_WARNING("[TileMap] (Runtime) Tilemap file does not exist: " << mapPath);
                }

                // If we don't have a valid map loaded but we have tile world size and default dimensions, create an empty map so the game can still run with a blank tilemap
                if (!entry.Map) {
                    entry.Map = std::make_shared<TileMap>(comp.TileWorldSize);
                    entry.Map->AddLayer(comp.DefaultWidth, comp.DefaultHeight);
                }
                
                // Log the result of the loading attempt
                if (entry.Map) {
                    LOG_INFO("[TileMap] (Runtime) Loaded tilemap: " << mapPath << " layers=" << entry.Map->LayerCount());
                }
            }

            // Next, resolve tileset paths and determine if we need to rebuild tilesets
            std::vector<std::string> tilesetPaths;

            // If the map loaded successfully, get tileset paths from the map dat
            // This allows the map to specify multiple tilesets and their paths
            if (entry.Map) {
                tilesetPaths = entry.Map->GetTilesetPaths();
            }

            // Resolve the tileset paths to actual file system paths, and check if they exist
            std::vector<std::string> resolvedTilesetPaths;
            resolvedTilesetPaths.reserve(tilesetPaths.size());

            // We prioritize the tileset paths specified in the map data
            for (const auto& path : tilesetPaths) {
                const std::string resolvedPath = ResolveProjectPathForLoad(path);
                // Only add to the list of tilesets if the resolved path is valid and the file exists
                if (!resolvedPath.empty() && std::filesystem::exists(resolvedPath)) {
                    resolvedTilesetPaths.push_back(resolvedPath);
                }
            }

            // If no valid tileset paths were found from the map data, but a legacy tileset path is provided, attempt to resolve and use that as a fallback
            if (resolvedTilesetPaths.empty() && legacyTilesetExists) {
                resolvedTilesetPaths.push_back(legacyTilesetPath);
            }

            // Determine if we need to rebuild the tilesets based on changes to the tileset paths or tile pixel size
            const bool tilesetsNeedRebuild = entry.TilePixelSize != comp.TilePixelSize || entry.TilesetPaths != resolvedTilesetPaths;

            // Rebuild tilesets if needed
            if (tilesetsNeedRebuild) {
                entry.Tilesets.clear();
                entry.TilesetPaths = resolvedTilesetPaths;
                entry.Tilesets.reserve(entry.TilesetPaths.size());

                // Build tilesets from the resolved paths
                // The BuildTilesetFromTexture function will attempt to load the texture and create a tileset based on the specified tile pixel size
                for (const auto& tilesetPath : entry.TilesetPaths) {
                    entry.Tilesets.push_back(BuildTilesetFromTexture(tilesetPath, comp.TilePixelSize));
                }

                // If we have a map but no valid tilesets, we can still run the game, but the tilemap will not render any tiles since it has 
                // no tileset to reference for tile definitions
                entry.TilePixelSize = comp.TilePixelSize;

                // Check if we have at least one valid tileset after attempting to build from the resolved paths
                bool hasValidTileset = false;
                for (const auto& tileset : entry.Tilesets) {
                    if (tileset) {
                        hasValidTileset = true;
                        break;
                    }
                }

                // If we don't have any valid tilesets from the map data or legacy path, log a warning
                // The tilemap will still function but won't render any tiles
                if (!hasValidTileset && legacyTilesetExists) {
                    entry.Tilesets.clear();
                    entry.TilesetPaths.clear();
                    entry.TilesetPaths.push_back(legacyTilesetPath);
                    entry.Tilesets.push_back(BuildTilesetFromTexture(legacyTilesetPath, comp.TilePixelSize));
                    hasValidTileset = (entry.Tilesets.back() != nullptr);
                    LOG_WARNING("[TileMap] (Runtime) Falling back to legacy tileset: " << legacyTilesetPath);
                }

                // For logging purposes
                LOG_INFO("[TileMap] (Runtime) Tileset rebuild entity " << entity.Index << " count=" << entry.Tilesets.size() << " valid=" << (hasValidTileset ? "yes" : "no"));
            }

            // Finally, update visibility and render layer info for this tilemap entry based on the current component state and entity transform
            glm::vec2 origin(0.0f, 0.0f);

            // If the entity has a LocalTransform, we need to calculate the world position of the tilemap to set the correct origin for rendering
            if (world.Has<ECS::Components::LocalTransform>(entity)) {
                const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);
                origin = glm::vec2(position.X, position.Y);
            }

            // Update the runtime entry with the latest visibility and layer information
            entry.Origin = origin;
            entry.Visible = comp.Visible && world.IsActiveInHierarchy(entity);
            entry.RenderLayerId = world.Has<ECS::Components::Layer>(entity) ? world.Get<ECS::Components::Layer>(entity).Id : 0;

            // Resolve Material2D for lighting
            entry.HasMaterial = false;
            if (world.Has<ECS::Components::Material2D>(entity)) {
                entry.Material = world.Get<ECS::Components::Material2D>(entity);
                entry.HasMaterial = true;
            }
        });

        // Remove any runtime entries for entities that no longer have a TileMapComponent (i.e. they were destroyed or had the component removed)
        for (auto it = m_runtimeTileMaps.begin(); it != m_runtimeTileMaps.end(); ) {
            if (!seen.contains(it->first)) {
                it = m_runtimeTileMaps.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    // Submits a runtime tile map entity for rendering during Game mode
    void RendererSystem::SubmitRuntimeTileMapEntity(Entity entity) {
        // Runtime tile maps only exist during Game mode; skip if we're in the editor
    // or if the renderer hasn't been initialized yet
        if (Engine::CORE->GetMode() != Engine::EngineMode::Game || !m_renderer) {
            return;
        }

        // Look up the cached runtime entry by entity index; the map is keyed on
        // index alone, so we still need to validate generation below
        const auto it = m_runtimeTileMaps.find(entity.Index);
        if (it == m_runtimeTileMaps.end()) {
            return;
        }

        const RuntimeTileMapEntry& entry = it->second;

        // A stale index slot from a destroyed entity can linger in the map until
        // the slot is reused, so generation mismatch means this handle is dead
        if (entry.Generation != entity.Generation) {
            return;
        }

        // Nothing to draw if the tile map is hidden, unloaded or has no tilesets
        // providing the actual tile image data
        if (!entry.Visible || !entry.Map || entry.Tilesets.empty()) {
            return;
        }

        // A map with no layers has no geometry to emit, skip to avoid a no-op submit
        if (entry.Map->LayerCount() == 0) {
            return;
        }

        // TileMapRenderer::Submit expects raw pointers, but the entry owns the
        // tilesets as unique_ptrs - build a temporary view without transferring ownership
        std::vector<const Tileset*> rawTilesets;
        rawTilesets.reserve(entry.Tilesets.size());
        for (const auto& tileset : entry.Tilesets) {
            rawTilesets.push_back(tileset.get());
        }

        // Pass nullptr for material when the entity has no material override so the
        // renderer falls back to the tileset's default shader/texture bindings
        m_tileMapRenderer.Submit(
            *entry.Map, rawTilesets, *m_renderer, entry.Origin,
            entry.HasMaterial ? &entry.Material : nullptr
        );
    }

    // Submits a debug tile map entity for rendering in editor modes only
    void RendererSystem::SubmitDebugTileMapEntity(World& world, Entity entity) {
        // Debug tile maps are editor-only overlays; suppress them entirely in Game
        // mode so they never leak into shipped builds
        if (Engine::CORE->GetMode() == Engine::EngineMode::Game || !m_renderer) {
            return;
        }

        // Early out before touching the world if there's nothing registered at all
        if (m_debugTileMaps.empty()) {
            return;
        }

        // Dead or inactive entities have no transform in the hierarchy, so rendering
        // them would produce garbage world-space positions
        if (!world.IsAlive(entity) || !world.IsActiveInHierarchy(entity)) {
            return;
        }

        // Respect the per-component visibility flag when the entity carries a
        // TileMapComponent: the debug entries still exist but the user hid them
        if (world.Has<Components::TileMapComponent>(entity)) {
            const auto& comp = world.Get<Components::TileMapComponent>(entity);
            if (!comp.Visible) {
                return;
            }
        }

        // Each entity may have contributed multiple debug entries (e.g. one per
        // physics layer), so we iterate all of them rather than assuming a 1:1 mapping
        TileMapRenderer tileRenderer;
        for (const auto& entry : m_debugTileMaps) {
            // Null source means the entry was registered but never fully initialized
            if (entry.SourceEntity.IsNull()) {
                continue;
            }

            // Index + generation together uniquely identify the entity; index alone
            // would incorrectly match a recycled slot from a different entity
            if (entry.SourceEntity.Index != entity.Index || entry.SourceEntity.Generation != entity.Generation) {
                continue;
            }

            // Can't render a tile map with no tilesets; tile IDs would have no
            // image data to resolve against
            if (entry.Tilesets.empty()) {
                continue;
            }

            // Same raw-pointer view pattern as the runtime path: borrow without
            // transferring ownership out of the unique_ptrs
            std::vector<const Tileset*> rawTilesets;
            rawTilesets.reserve(entry.Tilesets.size());
            for (const auto& ts : entry.Tilesets) {
                rawTilesets.push_back(ts.get());
            }

            // Pull the material directly from the ECS component rather than a cached
            // entry field, since debug renders reflect live component state in the editor
            const ECS::Components::Material2D* mat = nullptr;
            if (world.Has<ECS::Components::Material2D>(entity)) {
                mat = &world.Get<ECS::Components::Material2D>(entity);
            }

            tileRenderer.Submit(entry.Map.get(), rawTilesets, *m_renderer, entry.Offset, mat);
        }
    }

    // Gather all active scene lights and upload one consolidated light buffer for the frame
    void RendererSystem::CollectLights(World& world) {
        m_lightManager.BeginFrame();

        struct LightInput {
            ECS::Entity Entity{};
            const Components::LocalTransform* Transform = nullptr;
            const Components::Light2D* Light = nullptr;
        };

        struct LightOutput {
            bool Valid = false;
            bool Directional = false;
            glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
            glm::vec3 Position = glm::vec3(0.0f);
            glm::vec3 Color = glm::vec3(1.0f);
            float Intensity = 1.0f;
            float Range = 1.0f;
        };

        std::vector<LightInput> inputs;
        world.Each<Components::LocalTransform, Components::Light2D>(
            [&](ECS::Entity e, const Components::LocalTransform& lt, const Components::Light2D& l) {
                inputs.push_back(LightInput{ e, &lt, &l });
            });

        std::vector<LightOutput> outputs(inputs.size());
        auto evaluateLightRange = [&](size_t begin, size_t end, uint32_t) {
            for (size_t i = begin; i < end; ++i) {
                const LightInput& in = inputs[i];
                LightOutput& out = outputs[i];

                if (!world.IsActiveInHierarchy(in.Entity) || !in.Transform || !in.Light) {
                    continue;
                }

                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, in.Entity, *in.Transform, position, rotation, scale);

                out.Valid = true;
                out.Color = glm::vec3(ToGlm(in.Light->Color));
                out.Intensity = in.Light->Intensity;

                if (in.Light->LightType == Components::Light2D::Type::Directional) {
                    out.Directional = true;
                    glm::vec3 dir(in.Light->Direction.X, in.Light->Direction.Y, in.Light->Direction.Z);
                    if (glm::dot(dir, dir) < 1e-8f) {
                        dir = glm::vec3(0.0f, -1.0f, 0.0f);
                    }
                    out.Direction = glm::normalize(dir);
                }
                else {
                    out.Directional = false;
                    out.Position = glm::vec3(position.X, position.Y, position.Z) +
                        glm::vec3(in.Light->Position.X, in.Light->Position.Y, in.Light->Position.Z);
                    out.Range = in.Light->Range;
                }
            }
        };

        if (inputs.size() < kRendererLightParallelThreshold) {
            evaluateLightRange(0u, inputs.size(), 0u);
        }
        else {
            Engine::Physics2D::Internal::ParallelForStatic(inputs.size(), kRendererPrepMaxWorkers, evaluateLightRange);
        }

        // Commit in deterministic input order so directional-light overwrite behavior stays stable.
        for (const LightOutput& out : outputs) {
            if (!out.Valid) {
                continue;
            }

            if (out.Directional) {
                m_lightManager.SetDirectionalLight(out.Direction, out.Color, out.Intensity);
            }
            else {
                m_lightManager.AddPointLight(out.Position, out.Range, out.Color, out.Intensity);
            }
        }

        m_lightManager.Upload();
    }

    // Bucket entities for ordered rendering
    void RendererSystem::BucketEntities(World& world,
        std::vector<std::vector<Entity>>& buckets,
        int& maxLayerId) {
        maxLayerId = -1;
        // Render per-layer passes
        world.Each<Components::Layer>([&](Entity, const Components::Layer& ly) {
            maxLayerId = std::max(static_cast<int>(ly.Id), maxLayerId);
            });

        buckets.clear();
        buckets.resize(std::max(1, maxLayerId + 1));

        std::vector<std::pair<uint16_t, Entity>> entries;
        world.Each<Components::LocalTransform, Components::Layer>(
            [&](Entity entity, Components::LocalTransform&, const Components::Layer& ly) {
                entries.emplace_back(ly.Id, entity);
            });

        if (entries.size() < kRendererBucketParallelThreshold) {
            for (const auto& entry : entries) {
                if (entry.first < buckets.size()) {
                    buckets[entry.first].push_back(entry.second);
                }
            }
            return;
        }

        std::vector<std::vector<std::pair<uint16_t, Entity>>> perWorker(kRendererPrepMaxWorkers);
        Engine::Physics2D::Internal::ParallelForStatic(entries.size(), kRendererPrepMaxWorkers,
            [&entries, &perWorker](size_t begin, size_t end, uint32_t workerIdx) {
                auto& local = perWorker[workerIdx];
                local.reserve(local.size() + (end - begin));
                for (size_t i = begin; i < end; ++i) {
                    local.push_back(entries[i]);
                }
            });

        // Merge by worker index to preserve static-partition global order.
        for (const auto& local : perWorker) {
            for (const auto& entry : local) {
                if (entry.first < buckets.size()) {
                    buckets[entry.first].push_back(entry.second);
                }
            }
        }
    }

    // Render bloom
    void RendererSystem::RenderBloom(Viewport& vp, float bloomRadius) {
        // Extract
        vp.BloomExtract->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.BloomExtract->Width(), vp.BloomExtract->Height());
        m_bloomExtractShader->use();
        m_bloomExtractShader->setUniform("uThreshold", 1.1f);
        m_bloomExtractShader->setUniform("uScene", 0);
        vp.HDR->BindColorTexture(0);
        m_renderer->drawFullscreenQuad();

        // Blur Horizontal
        vp.BloomBlur->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.BloomBlur->Width(), vp.BloomBlur->Height());
        m_bloomBlurShader->use();
        m_bloomBlurShader->setUniform("uHorizontal", 1);
        m_bloomBlurShader->setUniform("uImage", 0);
        m_bloomBlurShader->setUniform("uRadius", bloomRadius);
        m_bloomBlurShader->setUniform("uSamples", std::max(12, static_cast<int>(bloomRadius * 0.6f)));
        m_bloomBlurShader->setUniform("uFalloff", 0.15f);
        vp.BloomExtract->BindColorTexture(0);
        m_renderer->drawFullscreenQuad();

        // Blur Vertical
        vp.BloomExtract->BindAndClear(0, 0, 0, 1);
        m_bloomBlurShader->setUniform("uHorizontal", 0);
        vp.BloomBlur->BindColorTexture(0);
        m_renderer->drawFullscreenQuad();

        // Unbind the current render target
        Framebuffer::Unbind();
    }

    /**
     * @brief Tone-map HDR scene into LDR and optionally blend bloom.
     * @param vp Viewport containing HDR/LDR/bloom framebuffers.
     * @param bloomEnabled True to blend bloom contribution; false to render HDR-only tone mapping.
     * @return void
     * @note Bloom texture is still bound when disabled, but strength is forced to 0.
     */
    void RendererSystem::ToneMap(Viewport& vp, const bool bloomEnabled) {
        vp.LDR->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.Size.x, vp.Size.y);

        m_bloomCombineShader->use();
        m_bloomCombineShader->setUniform("uScene", 0);
        m_bloomCombineShader->setUniform("uBloomBlur", 1);
        m_bloomCombineShader->setUniform("uExposure", 1.3f);
        m_bloomCombineShader->setUniform("uBloomStrength", bloomEnabled ? 5.2f : 0.0f);
        m_bloomCombineShader->setUniform("uGamma", 1.5f);

        vp.HDR->BindColorTexture(0, 0);
        vp.BloomExtract->BindColorTexture(0, 1);
        m_renderer->drawFullscreenQuad();

        // Unbind the current render target
        Framebuffer::Unbind();
    }

    // Render scene to hdr
    void RendererSystem::RenderSceneToHDR(World& world, Viewport& vp, const glm::mat4& viewProj,
        const std::vector<std::vector<Entity>>& buckets,
        int maxLayerId) {
        vp.HDR->BindAndClear(0.025f, 0.028f, 0.032f, 1.0f);
        glViewport(0, 0, vp.Size.x, vp.Size.y);

        auto* layerManager = world.GetLayerManager();
        std::vector<uint16_t> renderOrder;
        if (layerManager) {
            renderOrder = layerManager->DrawOrder();
        }
        else {
            for (int layer = 0; layer <= maxLayerId; ++layer)
                renderOrder.push_back(static_cast<uint16_t>(layer));
        }

        std::vector<glm::vec2> transformedCorners;
        std::vector<Entity> sortedLayerEntities;

        for (uint16_t layerId : renderOrder) {
            if (layerManager) {
                const auto& layerData = layerManager->Get(layerId);
                if (!layerData.renderEnabled || !layerData.editorVisible) continue;
            }

            int layer = static_cast<int>(layerId);
            if (layer >= static_cast<int>(buckets.size())) continue;

            const auto& sourceList = buckets[layer];
            sortedLayerEntities.assign(sourceList.begin(), sourceList.end());
            auto& list = sortedLayerEntities;
            // Finalize rendering pass state
            std::sort(list.begin(), list.end(), [&](const Entity& A, const Entity& B) {
                const auto* zA = world.TryGet<Components::ZIndex2D>(A);
                const auto* zB = world.TryGet<Components::ZIndex2D>(B);
                const int za = zA ? zA->ZOrder : 0;
                const int zb = zB ? zB->ZOrder : 0;
                if (za != zb) return za < zb;
                return A.Index < B.Index;
                });

            // SDF circles
            m_sdfCircleShader->use();
            m_sdfCircleShader->setMat4("uViewProj", viewProj);
            m_sdfCircleShader->setUniform("uStrokePx", 0.0f);
            m_sdfCircleShader->setUniform("uUseOverrideColor", 0);
            m_renderer->beginFrame();

            for (Entity entity : list) {
                if (!world.IsActiveInHierarchy(entity)) continue;
                const auto* sc = world.TryGet<Components::ShapeCircle2D>(entity);
                if (!sc) continue;
                const auto* lt = world.TryGet<Components::LocalTransform>(entity);
                if (!lt) continue;

                Vector3D position, scale; Quaternion rotation;
                GetRenderTransform(world, entity, *lt, position, rotation, scale);

                // Submit circle geometry
                DebugDraw2D::Circle(*m_renderer,
                    ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sc->Offset),
                    sc->Radius * ((scale.X + scale.Y) * 0.5f),
                    ToGlm(sc->Color),
                    sc->Filled ? 0.0f : sc->Thickness, 0);
            }
            m_renderer->endFrame();

            // Everything else
            m_shader->use();
            m_shader->setMat4("uViewProj", viewProj);
            m_shader->setUniform("uPicking", 0);
            m_shader->setUniform("uLightingEnabled", 1);
            m_lightManager.Bind(*m_shader);
            m_renderer->beginFrame();

            // Tilemap
            if (m_debugTileMap && m_debugTileset) {
                TileMapRenderer tileRenderer;
           
                // Wrap the debug tileset into a vector, since Submit expects multiple tilesets
                const std::vector<const Tileset*> tilesets = { &m_debugTileset->get() };

                // Submit the debug tilemap for rendering
                tileRenderer.Submit(*m_debugTileMap, tilesets, *m_renderer, m_debugTileMapOffset);
            }

            // Render only legacy debug tilemaps that are not owned by an entity
            // Entity-owned debug tilemaps are submitted in Z-sorted order below
            if (!m_debugTileMaps.empty()) {
                TileMapRenderer tileRenderer;
                for (const auto& entry : m_debugTileMaps) {
                    if (!entry.SourceEntity.IsNull()) {
                        continue;
                    }
                    if (entry.Tilesets.empty()) {
                        continue;
                    }

                    std::vector<const Tileset*> rawTilesets;
                    rawTilesets.reserve(entry.Tilesets.size());
                    for (const auto& ts : entry.Tilesets) {
                        rawTilesets.push_back(ts.get());
                    }
                    tileRenderer.Submit(entry.Map.get(), rawTilesets, *m_renderer, entry.Offset, nullptr);
                }
            }

            for (Entity entity : list) {
                if (!world.IsActiveInHierarchy(entity)) continue;
                if (world.TryGet<Components::ShapeCircle2D>(entity)) continue;

                // Keep tilemap draw order aligned with Z-sorted entity order
                SubmitRuntimeTileMapEntity(entity);
                SubmitDebugTileMapEntity(world, entity);

                // Boid flock entity  flush batch, draw instanced at correct Z
                if (world.TryGet<Components::BoidFlock>(entity)) {
                    m_renderer->endFrame();

                    if (m_boidSystem && m_boidShader) {
                        m_boidShader->use();
                        m_boidShader->setMat4("uViewProj", viewProj);
                        m_lightManager.Bind(*m_boidShader);
                        m_boidSystem->DrawFlockForEntity(entity.Index, *m_boidShader);
                    }

                    m_shader->use();
                    m_shader->setMat4("uViewProj", viewProj);
                    m_shader->setUniform("uPicking", 0);
                    m_shader->setUniform("uLightingEnabled", 1);
                    m_lightManager.Bind(*m_shader);
                    m_renderer->beginFrame();
                    continue;
                }

                // Particle emitter — flush batch, draw instanced at correct Z
                if (world.TryGet<Components::ParticleEmitter>(entity)) {
                    m_renderer->endFrame();

                    if (m_particleSystem && m_particleShader) {
                        m_particleShader->use();
                        m_particleShader->setMat4("uViewProj", viewProj);
                        m_lightManager.Bind(*m_particleShader);
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        m_particleSystem->DrawEmitterForEntity(entity.Index, *m_particleShader, world);
                    }

                    m_shader->use();
                    m_shader->setMat4("uViewProj", viewProj);
                    m_shader->setUniform("uPicking", 0);
                    m_shader->setUniform("uLightingEnabled", 1);
                    m_lightManager.Bind(*m_shader);
                    m_renderer->beginFrame();
                    continue;
                }

                auto* lt = world.TryGet<Components::LocalTransform>(entity);
                if (!lt) {
                    continue;
                }
                Vector3D position, scale; Quaternion rotation;
                GetRenderTransform(world, entity, *lt, position, rotation, scale);

                // Boxes
                if (world.Has<Components::ShapeBox2D>(entity)) {
                    const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                    const float rotationAngle = 2.0f * std::acos(rotation.W);
                    const bool hasRotation = std::abs(rotationAngle) > 0.01f;

                    if (!hasRotation) {
                        const glm::vec2 halfExtents = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                        const glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                        const glm::vec2 min = center - halfExtents;
                        const glm::vec2 max = center + halfExtents;
                        if (sb.Filled) DebugDraw2D::RectFill(*m_renderer, min, max, ToGlm(sb.Color), 0);
                        else DebugDraw2D::RectStroke(*m_renderer, min, max, sb.Thickness, ToGlm(sb.Color), 0);
                    }
                    else {
                        const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                        transformedCorners.clear();
                        const Vector2D he = sb.HalfExtents;
                        const Vector3D corners[4] = { {-he.X,-he.Y,0},{he.X,-he.Y,0},{he.X,he.Y,0},{-he.X,he.Y,0} };
                        for (auto c : corners) {
                            const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                            transformedCorners.push_back(ToGlm(Vector2D{ hc.X, hc.Y }) + ToGlm(sb.Offset));
                        }
                        if (sb.Filled) DebugDraw2D::Polygon(*m_renderer, transformedCorners, ToGlm(sb.Color), 0);
                        else for (int i = 0; i < 4; ++i)
                            // Submit line geometry
                            DebugDraw2D::Line(*m_renderer, transformedCorners[i], transformedCorners[(i + 1) % 4], sb.Thickness, ToGlm(sb.Color), 0);
                    }
                }

                // Lines
                if (world.Has<Components::ShapeLine2D>(entity)) {
                    const auto& sl = world.Get<Components::ShapeLine2D>(entity);
                    const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                    const Vector4D worldA = m * Vector4D{ sl.A.X, sl.A.Y, 0.0f, 1.0f };
                    const Vector4D worldB = m * Vector4D{ sl.B.X, sl.B.Y, 0.0f, 1.0f };
                    // Submit line geometry
                    DebugDraw2D::Line(*m_renderer, ToGlm(Vector2D{ worldA.X, worldA.Y }), ToGlm(Vector2D{ worldB.X, worldB.Y }), sl.Thickness, ToGlm(sl.Color), 0);
                }

                // Sprites
                if (world.Has<Components::SpriteRenderer2D>(entity)) {
                    const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                    const float angleZ = std::atan2(
                        2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                        1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z));

                    GLuint normalTexId = 0, mraTexId = 0;
                    float metallic = 0.0f, smoothness = 0.5f, aoStrength = 1.0f, normalStrength = 1.0f;
                    uint32_t flags = 0;

                    if (const auto* mat = world.TryGet<Components::Material2D>(entity)) {
                        normalTexId = mat->NormalTextureId;
                        mraTexId = mat->MRA_TextureId;
                        metallic = mat->Metallic;
                        smoothness = mat->Smoothness;
                        aoStrength = mat->AOStrength;
                        normalStrength = mat->NormalStrength;
                        flags = mat->Flags;
                        if (normalTexId == 0) normalTexId = sr.NormalTextureId;
                    }

                    m_renderer->submitSprite({
                        ToGlm(Vector2D{position.X, position.Y}),
                        ToGlm(Vector2D{scale.X, scale.Y}),
                        {sr.Offset.X, sr.Offset.Y, sr.Offset.X + sr.Tiling.X, sr.Offset.Y + sr.Tiling.Y},
                        ToGlm(sr.Color), sr.TextureId, angleZ, 1.0f,
                        sr.EmissiveTextureId, sr.EmissiveStrength, sr.Width, sr.Height,
                        normalTexId, mraTexId, metallic, smoothness, aoStrength, normalStrength,
                        flags,
                        sr.TextureFilter
                        });
                }
            }
              m_renderer->endFrame();
          }
        RenderWorldGUI(viewProj);

        // Unbind the current render target
        Framebuffer::Unbind();
    }

    // Render wireframes
    void RendererSystem::RenderWireframes(Viewport& vp, const glm::mat4& viewProj) {
        if (m_wireframeQueue.empty()) return;

        vp.LDR->Bind();
        glViewport(0, 0, vp.Size.x, vp.Size.y);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (const auto& sub : m_wireframeQueue) {
            if (sub.type == WireframeSubmission::Type::Circle) {
                m_sdfCircleShader->use();
                m_sdfCircleShader->setMat4("uViewProj", viewProj);
                m_sdfCircleShader->setUniform("uPicking", 0);
                m_renderer->beginFrame();
                // Submit circle geometry
                DebugDraw2D::Circle(*m_renderer, sub.center, sub.radius, sub.color, sub.filled ? 0.0f : sub.thickness, 0);
                m_renderer->endFrame();
            }
            else {
                m_shader->use();
                m_shader->setMat4("uViewProj", viewProj);
                m_shader->setUniform("uPicking", 0);
                m_shader->setUniform("uLightingEnabled", 0);
                m_renderer->beginFrame();

                if (sub.type == WireframeSubmission::Type::Quad && sub.vertices.size() == 4) {
                    if (sub.filled) DebugDraw2D::RectFill(*m_renderer, sub.vertices[0], sub.vertices[2], sub.color, 0);
                    else DebugDraw2D::RectStroke(*m_renderer, sub.vertices[0], sub.vertices[2], sub.thickness, sub.color, 0);
                }
                else if (sub.type == WireframeSubmission::Type::Line && sub.vertices.size() == 2) {
                    // Submit line geometry
                    DebugDraw2D::Line(*m_renderer, sub.vertices[0], sub.vertices[1], sub.thickness, sub.color, 0);
                }
                else if (sub.type == WireframeSubmission::Type::Polygon && sub.vertices.size() >= 2) {
                    if (sub.filled && sub.vertices.size() >= 3) DebugDraw2D::Polygon(*m_renderer, sub.vertices, sub.color, 0);
                    else for (size_t i = 0; i < sub.vertices.size(); ++i) {
                        size_t next = sub.closed ? (i + 1) % sub.vertices.size() : i + 1;
                        if (next < sub.vertices.size())
                            // Submit line geometry
                            DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[next], sub.thickness, sub.color, 0);
                    }
                }
                m_renderer->endFrame();
            }
        }

        // Unbind the current render target
        Framebuffer::Unbind();
    }

    // Render overlay quads
    void RendererSystem::RenderOverlayQuads(Viewport& vp, const glm::mat4& viewProj) {
        if (m_overlayQuadQueue.empty()) return;

        vp.LDR->Bind();
        glViewport(0, 0, vp.Size.x, vp.Size.y);

        // Enable blending for overlay quads
        GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Setup shader
        if (m_shader) {
            m_shader->use();
            m_shader->setMat4("uViewProj", viewProj);
            m_shader->setUniform("uPicking", 0);
            m_shader->setUniform("uLightingEnabled", 0);
        }

        // Render quads
        m_renderer->beginFrame();
        for (const auto& quad : m_overlayQuadQueue) {
            m_renderer->submitQuad(
                quad.center,
                quad.size,
                quad.textureId,
                quad.uvRect,
                quad.color,
                quad.rotation,
                1.0f,
                0
            );
        }
        m_renderer->endFrame();

        if (!blendWasEnabled) glDisable(GL_BLEND);

        // Unbind the current render target
        Framebuffer::Unbind();
    }

    // Render gui
    void RendererSystem::RenderGUI(Viewport& vp) {
        if (m_guiPanelQueue.empty() && m_guiTextQueue.empty() && m_guiImageQueue.empty()) return;

        Renderer* guiRenderer = m_guiRenderer ? m_guiRenderer.get() : m_renderer.get();
        if (!guiRenderer) {
            return;
        }

        vp.LDR->Bind();
        glViewport(0, 0, vp.Size.x, vp.Size.y);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float w = static_cast<float>(vp.Size.x);
        const float h = static_cast<float>(vp.Size.y);
        glm::mat4 screenOrtho = glm::ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

        // Layout is produced once before render; remap it into the current viewport size so
        // screen-space GUI scales with viewport/window resizing in multi-viewport rendering
        GUIViewport layoutViewport = m_guiViewport;
        if (!layoutViewport.Active || layoutViewport.Size.X <= 0.0f || layoutViewport.Size.Y <= 0.0f) {
            layoutViewport.Origin = { 0.0f, 0.0f };
            layoutViewport.Size = m_renderTargetSize;
        }
        const float layoutW = std::max(1.0f, layoutViewport.Size.X);
        const float layoutH = std::max(1.0f, layoutViewport.Size.Y);
        const float guiScaleX = w / layoutW;
        const float guiScaleY = h / layoutH;
        const float layoutOriginX = layoutViewport.Origin.X;
        const float layoutOriginY = layoutViewport.Origin.Y;
        auto scalePos = [&](const Vector2D& p) {
            return Vector2D{
                (p.X - layoutOriginX) * guiScaleX,
                (p.Y - layoutOriginY) * guiScaleY
            };
        };
        auto scaleSize = [&](const Vector2D& s) {
            return Vector2D{ s.X * guiScaleX, s.Y * guiScaleY };
        };

        // Panels
        if (!m_guiPanelQueue.empty()) {
            Shader* guiShader = m_guiShader ? m_guiShader.get() : m_shader.get();
            if (!guiShader) return;
            guiShader->use();
            guiShader->setMat4("uViewProj", screenOrtho);
            guiShader->setUniform("uGamma", 1.5f);
            guiRenderer->beginFrame();
            for (const auto& panel : m_guiPanelQueue) {
                const Vector2D panelPos = scalePos(panel.position);
                const Vector2D panelSize = scaleSize(panel.size);
                glm::vec2 center(panelPos.X + panelSize.X * 0.5f, panelPos.Y + panelSize.Y * 0.5f);
                glm::vec2 size(panelSize.X, panelSize.Y);
                glm::vec4 color(panel.color.R, panel.color.G, panel.color.B, panel.color.A);
                guiRenderer->submitQuad(center, size, 0, { 0,0,1,1 }, color, panel.rotation, 1.0f, 0, 0u, 0.0f);
            }
            guiRenderer->endFrame();
        }

        // Images
        if (!m_guiImageQueue.empty()) {
            Shader* guiShader = m_guiShader ? m_guiShader.get() : m_shader.get();
            if (!guiShader) return;
            guiShader->use();
            guiShader->setMat4("uViewProj", screenOrtho);
            guiShader->setUniform("uGamma", 1.5f);
            guiRenderer->beginFrame();
            for (const auto& image : m_guiImageQueue) {
                const Vector2D imagePos = scalePos(image.position);
                const Vector2D imageSize = scaleSize(image.size);
                glm::vec2 center(imagePos.X + imageSize.X * 0.5f, imagePos.Y + imageSize.Y * 0.5f);
                glm::vec2 size(imageSize.X, imageSize.Y);
                glm::vec4 color(image.color.R, image.color.G, image.color.B, image.color.A);
                // GUI projection uses Y-down; flip V to keep textures upright
                glm::vec4 uvRect(image.uvRect.X, image.uvRect.W, image.uvRect.Z, image.uvRect.Y);
                guiRenderer->submitQuad(center, size, image.textureId, uvRect, color, image.rotation, 1.0f, 0, 0u, 0.0f,
                    0, 0, 0.0f, 0.5f, 1.0f, 1.0f, 0, image.textureFilter);
            }
            guiRenderer->endFrame();
        }

        // Text
        if (!m_guiTextQueue.empty()) {
            glm::mat4 textOrtho = glm::ortho(0.0f, w, 0.0f, h, -1.0f, 1.0f);
            m_textShader->use();
            m_textShader->setMat4("uProjection", textOrtho);
            guiRenderer->beginFrame();
            for (const auto& text : m_guiTextQueue) {
                if (text.text.empty()) continue;
                std::string fontPath = text.fontPath.empty() ? "assets/fonts/Roboto/static/Roboto-Regular.ttf" : text.fontPath;
                const Vector2D textPosScaled = scalePos(text.position);
                // Keep GUI text size in absolute pixels regardless of viewport remapping
                const float textPixelSize = text.pixelSize;
                auto font = RM.GetFont(fontPath, std::max(1, static_cast<int>(textPixelSize)));
                if (!font) continue;
                const float scale = textPixelSize / static_cast<float>(font->getPixelSize());
                const float ascent = font->getAscent() * scale;
                glm::vec2 pos(textPosScaled.X, h - textPosScaled.Y - ascent);
                glm::vec4 color(text.color.R, text.color.G, text.color.B, text.color.A);
                guiRenderer->submitText(*font, text.text, pos, color, textPixelSize);
            }
            guiRenderer->endFrame();
        }

        // Unbind the current render target
        Framebuffer::Unbind();
    }

    // Render queued world-space GUI primitives so they move with the scene camera
    void RendererSystem::RenderWorldGUI(const glm::mat4& viewProj) {
        if (m_worldGuiPanelQueue.empty() && m_worldGuiTextQueue.empty() && m_worldGuiImageQueue.empty()) return;
        if (!m_renderer) return;

        // Preserve caller blend state because this pass can run from different render paths
        GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Panels
        if (!m_worldGuiPanelQueue.empty()) {
            if (m_shader) {
                m_shader->use();
                m_shader->setMat4("uViewProj", viewProj);
                m_shader->setUniform("uPicking", 0);
                m_shader->setUniform("uLightingEnabled", 0);
            }
            m_renderer->beginFrame();
            for (const auto& panel : m_worldGuiPanelQueue) {
                const glm::vec2 center(panel.position.X + panel.size.X * 0.5f,
                                       panel.position.Y + panel.size.Y * 0.5f);
                const glm::vec2 size(panel.size.X, panel.size.Y);
                const glm::vec4 color(panel.color.R, panel.color.G, panel.color.B, panel.color.A);
                m_renderer->submitQuad(center, size, 0, { 0.0f, 0.0f, 1.0f, 1.0f }, color, panel.rotation, 1.0f, 0, 0u, 0.0f);
            }
            m_renderer->endFrame();
        }

        // Images
        if (!m_worldGuiImageQueue.empty()) {
            if (m_shader) {
                m_shader->use();
                m_shader->setMat4("uViewProj", viewProj);
                m_shader->setUniform("uPicking", 0);
                m_shader->setUniform("uLightingEnabled", 0);
            }
            m_renderer->beginFrame();
            for (const auto& image : m_worldGuiImageQueue) {
                const glm::vec2 center(image.position.X + image.size.X * 0.5f,
                                       image.position.Y + image.size.Y * 0.5f);
                const glm::vec2 size(image.size.X, image.size.Y);
                const glm::vec4 color(image.color.R, image.color.G, image.color.B, image.color.A);
                const glm::vec4 uvRect(image.uvRect.X, image.uvRect.Y, image.uvRect.Z, image.uvRect.W);
                m_renderer->submitQuad(center, size, image.textureId, uvRect, color, image.rotation, 1.0f, 0, 0u, 0.0f,
                    0, 0, 0.0f, 0.5f, 1.0f, 1.0f, 0, image.textureFilter);
            }
            m_renderer->endFrame();
        }

        // Text
        if (!m_worldGuiTextQueue.empty()) {
            if (m_textShader) {
                // World-space text shares the same camera projection as world geometry
                m_textShader->use();
                m_textShader->setMat4("uProjection", viewProj);
            }
            m_renderer->beginFrame();
            for (const auto& text : m_worldGuiTextQueue) {
                if (text.text.empty()) continue;
                const std::string fontPath = text.fontPath.empty()
                    ? std::string("assets/fonts/Roboto/static/Roboto-Regular.ttf")
                    : text.fontPath;

                // Convert world-size text request into pixel glyph size for font atlas lookup
                const float fontPixelSize = UnitsToPixels(text.pixelSize);
                auto font = RM.GetFont(fontPath, std::max(1, static_cast<int>(std::round(fontPixelSize))));
                if (!font) continue;
                const float scale = text.pixelSize / static_cast<float>(font->getPixelSize());
                const float ascent = font->getAscent() * scale;
                const glm::vec2 textPos(text.position.X, text.position.Y - ascent);
                const glm::vec4 color(text.color.R, text.color.G, text.color.B, text.color.A);
                m_renderer->submitText(*font, text.text, textPos, color, text.pixelSize);
            }
            m_renderer->endFrame();
        }

        if (!blendWasEnabled) glDisable(GL_BLEND);
    }

    // Render picking
    void RendererSystem::RenderPicking(World& world, Viewport& vp, const glm::mat4& viewProj,
        const std::vector<std::vector<Entity>>& buckets) {
        // Only run if there are pending pick requests
        if (m_pendingPickRequests.empty() && !m_currentPickRequest.has_value() && !m_inFlightPick.has_value())
            return;

        // Process in-flight pick from last frame
        if (m_inFlightPick.has_value()) {
            const int idx = m_inFlightPick->PBOIndex;
            if (idx >= 0 && idx < 2) {
                m_pbos[idx].Bind(GL_PIXEL_PACK_BUFFER);
                void* mapped = m_pbos[idx].Map(GL_READ_ONLY);
                if (mapped) {
                    uint8_t* bytes = static_cast<uint8_t*>(mapped);
                    uint32_t encoded = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
                    uint32_t pickedEntity = (encoded == 0) ? INVALID_ENTITY_ID : (encoded - 1);
                    m_completedPickResults[m_inFlightPick->RequestId] = pickedEntity;
                    m_pbos[idx].Unmap();
                }
                m_pbos[idx].Unbind(GL_PIXEL_PACK_BUFFER);
            }
            m_inFlightPick.reset();
        }

        // Dequeue next request
        if (!m_currentPickRequest.has_value() && !m_pendingPickRequests.empty()) {
            m_currentPickRequest = m_pendingPickRequests.front();
            m_pendingPickRequests.pop();
        }

        if (!m_currentPickRequest.has_value()) return;

        // Resize picking FBO if needed
        if (vp.PickingFBO->Width() != vp.Size.x || vp.PickingFBO->Height() != vp.Size.y)
            vp.PickingFBO->Resize(vp.Size.x, vp.Size.y, false, false);

        vp.PickingFBO->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.Size.x, vp.Size.y);
        glDisable(GL_BLEND);

        // Render circles with ID colors using SDF shader
        m_sdfCircleShader->use();
        m_sdfCircleShader->setMat4("uViewProj", viewProj);
        m_sdfCircleShader->setUniform("uPicking", 1);
        m_renderer->beginFrame();
        for (const auto& bucket : buckets) {
            for (Entity entity : bucket) {
                if (!world.IsActiveInHierarchy(entity)) continue;
                if (!world.Has<Components::ShapeCircle2D>(entity)) continue;

                uint32_t id = entity.Index + 1;
                glm::vec4 idColor(
                    ((id >> 0) & 0xFF) / 255.0f,
                    ((id >> 8) & 0xFF) / 255.0f,
                    ((id >> 16) & 0xFF) / 255.0f,
                    1.0f
                );

                const auto& lt = world.Get<Components::LocalTransform>(entity);
                Vector3D position, scale; Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                const auto& sc = world.Get<Components::ShapeCircle2D>(entity);
                // Submit circle geometry
                DebugDraw2D::Circle(
                    *m_renderer,
                    ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sc.Offset),
                    sc.Radius * ((scale.X + scale.Y) * 0.5f),
                    idColor,
                    0.0f,
                    0
                );
            }
        }
        m_renderer->endFrame();

        // Render entities with ID colors (simplified - just sprites/boxes)
        m_shader->use();
        m_shader->setMat4("uViewProj", viewProj);
        m_shader->setUniform("uPicking", 1);
        m_shader->setUniform("uLightingEnabled", 0);
        m_renderer->beginFrame();

        for (const auto& bucket : buckets) {
            for (Entity entity : bucket) {
                if (!world.IsActiveInHierarchy(entity)) continue;

                uint32_t id = entity.Index + 1;
                glm::vec4 idColor(((id >> 0) & 0xFF) / 255.0f, ((id >> 8) & 0xFF) / 255.0f, ((id >> 16) & 0xFF) / 255.0f, 1.0f);

                const auto& lt = world.Get<Components::LocalTransform>(entity);
                Vector3D position, scale; Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                if (world.Has<Components::SpriteRenderer2D>(entity)) {
                    const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                    float angleZ = std::atan2(2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                        1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z));
                    m_renderer->submitSprite({ ToGlm(Vector2D{position.X, position.Y}), ToGlm(Vector2D{scale.X, scale.Y}),
                        {sr.Offset.X, sr.Offset.Y, sr.Offset.X + sr.Tiling.X, sr.Offset.Y + sr.Tiling.Y},
                        idColor, static_cast<GLuint>(-1), angleZ, 1.0f,
                        0, 0.0f,
                        0, 0,
                        0, 0,
                        0.0f, 0.5f, 1.0f, 1.0f,
                        0,
                        sr.TextureFilter });
                }

                if (world.Has<Components::ShapeBox2D>(entity)) {
                    const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                    glm::vec2 he = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                    glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                    // Submit filled rectangle geometry
                    DebugDraw2D::RectFill(*m_renderer, center - he, center + he, idColor, static_cast<GLuint>(-1));
                }

                if (world.Has<Components::ShapeLine2D>(entity)) {
                    const auto& sl = world.Get<Components::ShapeLine2D>(entity);
                    const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                    const Vector4D worldA = m * Vector4D{ sl.A.X, sl.A.Y, 0.0f, 1.0f };
                    const Vector4D worldB = m * Vector4D{ sl.B.X, sl.B.Y, 0.0f, 1.0f };
                    // Submit line geometry
                    DebugDraw2D::Line(
                        *m_renderer,
                        ToGlm(Vector2D{ worldA.X, worldA.Y }),
                        ToGlm(Vector2D{ worldB.X, worldB.Y }),
                        sl.Thickness,
                        idColor,
                        static_cast<GLuint>(-1)
                    );
                }
            }
        }
        m_renderer->endFrame();

        // Read pixel (convert screen coords to local viewport coords)
        const float localX = m_currentPickRequest->ScreenX - m_currentPickRequest->ViewportPos.x;
        const float localY = m_currentPickRequest->ScreenY - m_currentPickRequest->ViewportPos.y;
        int readX = glm::clamp(static_cast<int>(localX), 0, vp.Size.x - 1);
        int readY = glm::clamp(static_cast<int>(vp.Size.y - localY - 1), 0, vp.Size.y - 1);

        m_pbos[m_currentPBO].Bind(GL_PIXEL_PACK_BUFFER);
        glReadPixels(readX, readY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        m_pbos[m_currentPBO].Unbind(GL_PIXEL_PACK_BUFFER);

        m_inFlightPick = InFlightPick{ m_currentPickRequest->RequestId, m_currentPBO };
        m_currentPickRequest.reset();
        m_currentPBO = 1 - m_currentPBO;

        glEnable(GL_BLEND);
        // Unbind the current render target
        Framebuffer::Unbind();
    }

    // Request pick
    uint32_t RendererSystem::RequestPick(float screenX, float screenY, const glm::vec2& viewportPos, const glm::vec2& viewportSize) {
        // Check if within viewport bounds
        if (screenX < viewportPos.x || screenX >= (viewportPos.x + viewportSize.x) ||
            screenY < viewportPos.y || screenY >= (viewportPos.y + viewportSize.y)) {
            LOG_DEBUG("[Renderer] RequestPick ignored, screen coordinates out of viewport bounds.");
            return ECS::Entity::NPOS32;
        }

        // Queue the request instead of rejecting it if one is pending
        uint32_t id = m_nextPickRequestId++;
        PendingPickRequest req;
        req.RequestId = id;
        req.ScreenX = screenX;
        req.ScreenY = screenY;
        req.ViewportPos = viewportPos;
        req.ViewportSize = viewportSize;
        m_pendingPickRequests.push(req);

        LOG_DEBUG("[Renderer] RequestPick id=" << id << " screen=(" << screenX << "," << screenY << ") viewport=(" << viewportPos.x << "," << viewportPos.y << "," << viewportSize.x << "," << viewportSize.y << ") - queued");
        return id;
    }

    // Try to get pick result
    bool RendererSystem::TryGetPickResult(uint32_t requestId, uint32_t& outEntityId) {
        auto it = m_completedPickResults.find(requestId);
        if (it == m_completedPickResults.end()) return false;
        outEntityId = it->second;
        m_completedPickResults.erase(it);
        return true;
    }

    // ============================================================
    // Wireframe/Debug Rendering API Implementations
    // ============================================================

    // Queue a rectangle primitive for the wireframe or fill overlay pass
    void RendererSystem::SubmitWireframeQuad(const glm::vec2& min, const glm::vec2& max,
                                              const glm::vec4& color, float thickness) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Quad;
        sub.vertices = {
            { min.x, min.y },
            { max.x, min.y },
            { max.x, max.y },
            { min.x, max.y }
        };
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = true;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    // Submit a filled quad for debug rendering
    void RendererSystem::SubmitFilledQuad(const glm::vec2& min, const glm::vec2& max,
                                          const glm::vec4& color) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Quad;
        sub.vertices = {
            { min.x, min.y },
            { max.x, min.y },
            { max.x, max.y },
            { min.x, max.y }
        };
        sub.color = color;
        sub.thickness = 0.0f;
        sub.closed = true;
        sub.filled = true;
        m_wireframeQueue.push_back(sub);
    }

    // Submit a wireframe circle for debug rendering
    void RendererSystem::SubmitWireframeCircle(const glm::vec2& center, float radius,
                                                const glm::vec4& color, float thickness) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Circle;
        sub.center = center;
        sub.radius = radius;
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = false;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    // Submit a wireframe polygon for debug rendering
    void RendererSystem::SubmitWireframePolygon(const glm::vec2* vertices, size_t vertexCount,
                                                 const glm::vec4& color, float thickness, bool closed) {
        if (!m_renderer || !vertices || vertexCount < 2) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Polygon;
        sub.vertices.assign(vertices, vertices + vertexCount);
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = closed;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    // Submit a wireframe line for debug rendering
    void RendererSystem::SubmitWireframeLine(const glm::vec2& p1, const glm::vec2& p2,
                                              const glm::vec4& color, float thickness) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Line;
        sub.vertices = { p1, p2 };
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = false;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    // Submit a wireframe mesh for debug rendering
    void RendererSystem::SubmitWireframeMesh(const glm::vec2* vertices, size_t vertexCount,
                                              const uint32_t* indices, size_t indexCount,
                                              const glm::vec4& color, float thickness) {
        if (!m_renderer || !vertices || vertexCount < 2) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Mesh;
        sub.vertices.assign(vertices, vertices + vertexCount);
        if (indices && indexCount > 0) {
            sub.indices.assign(indices, indices + indexCount);
        }
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = false;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    // Submit an overlay quad
    void RendererSystem::SubmitOverlayQuad(const glm::vec2& center,
                                           const glm::vec2& size,
                                           GLuint textureId,
                                           const glm::vec4& uvRect,
                                           const glm::vec4& color,
                                           float rotation) {
        if (!m_renderer) return;

        OverlayQuadSubmission sub;
        sub.center = center;
        sub.size = size;
        sub.textureId = textureId;
        sub.uvRect = uvRect;
        sub.color = color;
        sub.rotation = rotation;
        m_overlayQuadQueue.push_back(sub);
    }

    // Submit a GUI panel draw call
    void RendererSystem::SubmitGUIPanel(const Vector2D& position, const Vector2D& size,
                                        const Color& color, float cornerRadius, float rotationRadians) {
        (void)cornerRadius;
        if (!m_renderer) return;

        // Queue GUI panel draw for the GUI pass
        GUIPanelSubmission submission;
        submission.position = position;
        submission.size = size;
        submission.color = color;
        submission.cornerRadius = cornerRadius;
        submission.rotation = rotationRadians;
        m_guiPanelQueue.push_back(submission);
    }

    // Submit a GUI image draw call
    void RendererSystem::SubmitGUIImage(const Vector2D& position, const Vector2D& size,
                                         uint32_t textureId, const Vector4D& uvRect, const Color& color,
                                         Graphics::TextureFilter textureFilter, float rotationRadians) {
        if (!m_renderer) return;

        // Queue GUI image/icon draw for the GUI pass
        GUIImageSubmission submission;
        submission.position = position;
        submission.size = size;
        submission.textureId = textureId;
        submission.uvRect = uvRect;
        submission.color = color;
        submission.textureFilter = textureFilter;
        submission.rotation = rotationRadians;
        m_guiImageQueue.push_back(submission);
    }

    // Submit a GUI text draw call
    void RendererSystem::SubmitGUIText(const Vector2D& position, const std::string& text,
        const std::string& fontPath, float pixelSize, const Color& color) {
        if (!m_renderer) return;

        // Queue GUI text draw for the GUI pass
        GUITextSubmission submission;
        submission.position = position;
        submission.text = text;
        submission.fontPath = fontPath;
        submission.pixelSize = pixelSize;
        submission.color = color;
        m_guiTextQueue.push_back(std::move(submission));
    }

    // Submit a world-space GUI panel draw call
    void RendererSystem::SubmitWorldGUIPanel(const Vector2D& position, const Vector2D& size,
        const Color& color, float cornerRadius, float rotationRadians) {
        (void)cornerRadius;
        if (!m_renderer) return;

        WorldGUIPanelSubmission submission;
        submission.position = position;
        submission.size = size;
        submission.color = color;
        submission.cornerRadius = cornerRadius;
        submission.rotation = rotationRadians;
        m_worldGuiPanelQueue.push_back(submission);
    }

    // Submit a world-space GUI image draw call
    void RendererSystem::SubmitWorldGUIImage(const Vector2D& position, const Vector2D& size,
        uint32_t textureId, const Vector4D& uvRect, const Color& color, Graphics::TextureFilter textureFilter, float rotationRadians) {
        if (!m_renderer) return;

        WorldGUIImageSubmission submission;
        submission.position = position;
        submission.size = size;
        submission.textureId = textureId;
        submission.uvRect = uvRect;
        submission.color = color;
        submission.textureFilter = textureFilter;
        submission.rotation = rotationRadians;
        m_worldGuiImageQueue.push_back(submission);
    }

    // Submit a world-space GUI text draw call
    void RendererSystem::SubmitWorldGUIText(const Vector2D& position, const std::string& text,
        const std::string& fontPath, float pixelSize, const Color& color) {
        if (!m_renderer) return;

        WorldGUITextSubmission submission;
        submission.position = position;
        submission.text = text;
        submission.fontPath = fontPath;
        submission.pixelSize = pixelSize;
        submission.color = color;
        m_worldGuiTextQueue.push_back(std::move(submission));
    }

    // Submit collider debug draw geometry
    void RendererSystem::SubmitColliderDebugDraw(ECS::World& world, uint32_t entityID,
        const glm::vec4& color) {
        if (entityID == ECS::Entity::NPOS32) {
            return;
        }

        ECS::Entity entity{ entityID };

        // Get world position/rotation/scale (respect WorldTransform if present)
        Vector3D worldPos{ 0.0f, 0.0f, 0.0f };
        Vector3D scale{ 1.0f, 1.0f, 1.0f };
        Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (world.Has<ECS::Components::LocalTransform>(entity)) {
            const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
            GetRenderTransform(world, entity, lt, worldPos, rotation, scale);
        }

        // Precompute 2D position and angle
        const glm::vec2 worldPos2D{ worldPos.X, worldPos.Y };
        const float entityAngleZ = std::atan2(
            2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
            1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
        );

        // Helper: Rotate a 2D vector by radians
        auto rotate2D = [](const glm::vec2& v, float radians) {
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            // Convert to a 2D vector
            return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
            };

        // Render 2D Box Collider
        if (world.Has<ECS::Components::BoxCollider2D>(entity)) {
            auto& collider = world.Get<ECS::Components::BoxCollider2D>(entity);

            // Compute box corners
            const glm::vec2 offset{ collider.Offset.X, collider.Offset.Y };
            const glm::vec2 rotatedOffset = rotate2D(offset, entityAngleZ);
            const glm::vec2 center = worldPos2D + rotatedOffset;
            const glm::vec2 halfExtents{ collider.HalfExtents.X * scale.X, collider.HalfExtents.Y * scale.Y };

            // Account for collider rotation
            const float boxAngle = entityAngleZ + collider.Rotation;
            const glm::vec2 right = rotate2D(glm::vec2(1.0f, 0.0f), boxAngle);
            const glm::vec2 up = rotate2D(glm::vec2(0.0f, 1.0f), boxAngle);

            // Define corners
            glm::vec2 corners[4];
            corners[0] = center + right * halfExtents.x + up * halfExtents.y;
            corners[1] = center - right * halfExtents.x + up * halfExtents.y;
            corners[2] = center - right * halfExtents.x - up * halfExtents.y;
            corners[3] = center + right * halfExtents.x - up * halfExtents.y;

            // Submit as polygon
            WireframeSubmission sub;
            sub.type = WireframeSubmission::Type::Polygon;
            sub.vertices.assign(corners, corners + 4);
            sub.color = color;
            sub.thickness = 1.0f;
            sub.closed = true;
            sub.filled = true;
            m_wireframeQueue.push_back(sub);
        }

        // Render 2D Circle Collider - render as polygon for accuracy
        if (world.Has<ECS::Components::CircleCollider2D>(entity)) {
            auto& collider = world.Get<ECS::Components::CircleCollider2D>(entity);

            // Compute circle center and radius
            const glm::vec2 offset{ collider.Offset.X, collider.Offset.Y };
            const glm::vec2 rotatedOffset = rotate2D(offset, entityAngleZ);
            const glm::vec2 center = worldPos2D + rotatedOffset;
            const float radius = collider.Radius * ((scale.X + scale.Y) * 0.5f);

            // Submit as filled circle
            WireframeSubmission sub;
            sub.type = WireframeSubmission::Type::Circle;
            sub.center = center;
            sub.radius = radius;
            sub.color = color;
            sub.thickness = 0.0f;
            sub.closed = false;
            sub.filled = true;
            m_wireframeQueue.push_back(sub);
        }
    }

    // Set debug tile map
    void RendererSystem::SetDebugTileMap(const TileMap& map, const Tileset& tileset, const glm::vec2& worldOffset)
    {
        m_debugTileMap = map;
        m_debugTileset = tileset;
        m_debugTileMapOffset = worldOffset;
    }

    // Set debug tile maps
    void RendererSystem::SetDebugTileMaps(const std::vector<DebugTileMapEntry>& maps)
    {
        m_debugTileMaps = maps; // Store references to editor-owned tilemaps for this frame.
    }

    // Clear debug tile map
    void RendererSystem::ClearDebugTileMap()
    {
        m_debugTileMap.reset();
        m_debugTileset.reset();
        m_debugTileMapOffset = glm::vec2(0.0f, 0.0f);
    }

    // Clear debug tile maps
    void RendererSystem::ClearDebugTileMaps()
    {
        m_debugTileMaps.clear(); // Clear multi-tilemap debug rendering state.
    }
}
