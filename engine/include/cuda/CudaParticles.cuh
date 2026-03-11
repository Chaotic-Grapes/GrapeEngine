/* Start Header *****************************************************************/
/*!
\file     CudaParticles.cuh
\author   Choi Meng Yew (100%)
\date     28th February 2026
\brief
CUDA particle system header. Defines ParticleParams for kernel configuration
and declares host-side launch wrappers. Uses the same CUDA-GL interop pattern
as CudaBoids — particles live in a mapped OpenGL VBO.

Particle layout (3 x float4 per particle = 48 bytes):
  [0] posVel:    pos.x, pos.y, vel.x, vel.y
  [1] lifeColor: life,  maxLife, colorR, colorG
  [2] sizeRot:   colorB, colorA, size, rotation

Simulation runs on CUDA-only SoA buffers (d_posVel, d_lifeColor, d_sizeRot).
After simulation, Interleave() packs the 3 SoA arrays into the mapped GL VBO
in interleaved format for instanced rendering.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#ifdef GRAPE_HAS_CUDA
#include <cuda_runtime.h>
#include <cstdint>

// ============================================================
// Emission shape
// ============================================================
enum class EmissionShape : int {
    Point = 0,    // All particles from a single point
    Circle = 1,    // Random position within radius
    Cone = 2,    // Directional cone (for geysers, jets)
    Line = 3     // Along a line segment (for surface bubbles)
};

// ============================================================
// Particle kernel parameters (passed by value to kernels)
// ============================================================
struct ParticleParams {
    // --- Emission ---
    float emitterX = 0.0f;
    float emitterY = 0.0f;
    float emissionAngle = -1.5708f; // radians, default = upward (-PI/2)
    float emissionSpread = 0.3f;     // cone half-angle in radians
    float emissionRadius = 0.0f;     // for Circle/Line shapes
    EmissionShape shape = EmissionShape::Point;

    // --- Lifetime ---
    float lifetimeMin = 1.0f;     // seconds
    float lifetimeMax = 3.0f;

    // --- Velocity ---
    float speedMin = 50.0f;
    float speedMax = 150.0f;

    // --- Physics ---
    float gravityX = 0.0f;
    float gravityY = 0.0f;     // positive = down in screen coords, set negative for upward buoyancy
    float drag = 0.0f;     // velocity *= (1 - drag * dt)
    float turbulence = 0.0f;     // random force magnitude per frame

    // --- Wobble (for bubbles) ---
    float wobbleFrequency = 0.0f;     // Hz — sinusoidal horizontal oscillation
    float wobbleAmplitude = 0.0f;     // world units

    // --- Size over life ---
    float sizeStart = 4.0f;
    float sizeEnd = 1.0f;

    // --- Rotation ---
    float rotationSpeedMin = 0.0f;     // radians/sec
    float rotationSpeedMax = 0.0f;

    // --- Color over life (RGBA, 0-1) ---
    float colorStartR = 1.0f;
    float colorStartG = 1.0f;
    float colorStartB = 1.0f;
    float colorStartA = 1.0f;

    float colorEndR = 1.0f;
    float colorEndG = 1.0f;
    float colorEndB = 1.0f;
    float colorEndA = 0.0f;     // fade out by default

    // --- Simulation ---
    float dt = 0.016f;
    float totalTime = 0.0f;     // for wobble phase
    int   maxParticles = 0;        // buffer capacity
    int   aliveCount = 0;        // current living particles (updated by host)
    unsigned int frameCount = 0;

    // --- Emission control ---
    int   emitCount = 0;        // particles to emit THIS frame (host computes from rate)
    int   burstCount = 0;        // one-shot burst (consumed after emit)

    // --- Tile collision (same grid as boids) ---
    uint8_t* collisionMasks = nullptr;
    int32_t  collisionWidth = 0;
    int32_t  collisionHeight = 0;
    int32_t  collisionOriginX = 0;
    int32_t  collisionOriginY = 0;
    float    tileSize = 0.0f;
    bool     dieOnCollision = false; // kill particle on tile hit
    float    bounciness = 0.0f;  // 0 = no bounce, 1 = perfect bounce

    // --- Bounds ---
    float boundsMinX = -500.0f;
    float boundsMinY = -500.0f;
    float boundsMaxX = 500.0f;
    float boundsMaxY = 500.0f;
    bool  killOutOfBounds = true;     // kill instead of wrap
};

// ============================================================
// Host-side launch wrappers
// ============================================================
namespace CudaParticles {

    // Update all alive particles (physics, color, size, lifetime)
    // posVel, lifeColor, sizeRot are SoA buffers, each maxParticles long
    void Update(float4* posVel,
        float4* lifeColor,
        float4* sizeRot,
        const ParticleParams& params);

    // Emit new particles, appending after aliveCount
    // Returns new alive count (host should store this)
    int Emit(float4* posVel,
        float4* lifeColor,
        float4* sizeRot,
        const ParticleParams& params);

    // Compact: remove dead particles (life <= 0) by streaming live ones
    // to front of buffer. Returns new alive count.
    int Compact(float4* posVel,
        float4* lifeColor,
        float4* sizeRot,
        int aliveCount,
        int maxParticles,
        int* d_aliveFlags,
        int* d_offsets,
        int* d_totalAlive,
        float4* d_tempPV,
        float4* d_tempLC,
        float4* d_tempSR,
        void* d_scanTemp,
        size_t scanTempBytes);

    // Interleave 3 SoA buffers into a single interleaved VBO buffer.
    // dst must be at least count * 3 float4s.
    // Layout per particle: [posVel][lifeColor][sizeRot] (stride = 48 bytes)
    void Interleave(float4* dst,
        const float4* posVel,
        const float4* lifeColor,
        const float4* sizeRot,
        int count);

    size_t GetScanTempBytes(int count, int* d_in, int* d_out);

} // namespace CudaParticles

#endif // GRAPE_HAS_CUDA
