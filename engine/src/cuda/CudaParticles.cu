/* Start Header *****************************************************************/
/*!
\file     CudaParticles.cu
\author   Choi Meng Yew
\date     28th February 2026
\brief
CUDA particle system kernels. Four kernels:
  1. emitKernel   — spawn new particles with randomized properties
  2. updateKernel — simulate physics, color/size interpolation, lifetime
  3. compactKernel + prefix scan — remove dead particles
  4. interleaveKernel — pack 3 SoA arrays into interleaved VBO

Uses the same CUDA-GL interop pattern as CudaBoids.

Particle layout (3 x float4 per particle, SoA):
  posVel[i]:    { pos.x, pos.y, vel.x, vel.y }
  lifeColor[i]: { life,  maxLife, colorR, colorG }
  sizeRot[i]:   { colorB, colorA, size, rotation }

TO-DO:
Please add ping-pong buffers!!!

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "cuda/CudaParticles.cuh"

#ifdef GRAPE_HAS_CUDA

#include <curand_kernel.h>
#include <cstdio>
#include <cub/cub.cuh>

__global__ void computeTotalAliveKernel(const int* __restrict__ offsets,
    const int* __restrict__ aliveFlags,
    int* __restrict__ totalAliveOut,
    int count)
{
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        int last = count - 1;
        int total = offsets[last] + aliveFlags[last];
        *totalAliveOut = total;
    }
}

// ============================================================
// Emit kernel — each thread spawns one particle
// ============================================================
__global__ void emitKernel(float4* __restrict__ posVel,
    float4* __restrict__ lifeColor,
    float4* __restrict__ sizeRot,
    ParticleParams params,
    int baseIndex)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.emitCount) return;

    int particleIdx = baseIndex + idx;
    if (particleIdx >= params.maxParticles) return;

    // Per-particle RNG
    curandState rng;
    curand_init((unsigned long long)params.frameCount, particleIdx, 0, &rng);

    // --- Position ---
    float px = params.emitterX;
    float py = params.emitterY;

    if (params.shape == EmissionShape::Circle) {
        float angle = curand_uniform(&rng) * 6.2831853f;
        float r = sqrtf(curand_uniform(&rng)) * params.emissionRadius;
        px += cosf(angle) * r;
        py += sinf(angle) * r;
    }
    else if (params.shape == EmissionShape::Line) {
        float t = curand_uniform(&rng) * 2.0f - 1.0f; // -1 to 1
        // Line perpendicular to emission angle
        float perpX = -sinf(params.emissionAngle);
        float perpY = cosf(params.emissionAngle);
        px += perpX * t * params.emissionRadius;
        py += perpY * t * params.emissionRadius;
    }

    // --- Velocity (cone direction) ---
    float baseAngle = params.emissionAngle;
    float spread = params.emissionSpread;
    float angle = baseAngle + (curand_uniform(&rng) * 2.0f - 1.0f) * spread;
    float speed = params.speedMin + curand_uniform(&rng) * (params.speedMax - params.speedMin);
    float vx = cosf(angle) * speed;
    float vy = sinf(angle) * speed;

    // --- Lifetime ---
    float life = params.lifetimeMin + curand_uniform(&rng) * (params.lifetimeMax - params.lifetimeMin);

    // --- Rotation ---
    float rotSpeed = params.rotationSpeedMin +
        curand_uniform(&rng) * (params.rotationSpeedMax - params.rotationSpeedMin);
    // Randomize rotation direction
    if (curand_uniform(&rng) > 0.5f) rotSpeed = -rotSpeed;
    float initRotation = curand_uniform(&rng) * 6.2831853f;

    // --- Write particle ---
    posVel[particleIdx] = make_float4(px, py, vx, vy);
    lifeColor[particleIdx] = make_float4(life, life, params.colorStartR, params.colorStartG);
    sizeRot[particleIdx] = make_float4(params.colorStartB, params.colorStartA,
        params.sizeStart, initRotation);
}

// ============================================================
// Update kernel — each thread updates one alive particle
// ============================================================
__global__ void updateKernel(float4* __restrict__ posVel,
    float4* __restrict__ lifeColor,
    float4* __restrict__ sizeRot,
    ParticleParams params)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.aliveCount) return;

    // Read current state
    float4 pv = posVel[idx];
    float4 lc = lifeColor[idx];
    float4 sr = sizeRot[idx];

    float life = lc.x;
    float maxLife = lc.y;

    // Already dead — skip (will be compacted)
    if (life <= 0.0f) return;

    // Decrement lifetime
    life -= params.dt;

    float2 pos = make_float2(pv.x, pv.y);
    float2 vel = make_float2(pv.z, pv.w);

    // --- Physics ---
    // Gravity / buoyancy
    vel.x += params.gravityX * params.dt;
    vel.y += params.gravityY * params.dt;

    // Drag
    if (params.drag > 0.0f) {
        float factor = 1.0f - params.drag * params.dt;
        if (factor < 0.0f) factor = 0.0f;
        vel.x *= factor;
        vel.y *= factor;
    }

    // Turbulence — deterministic per-particle noise
    if (params.turbulence > 0.0f) {
        unsigned int hash = (idx * 2654435761u) ^ (params.frameCount * 2246822519u);
        float noiseX = ((float)(hash % 1000u) / 1000.0f - 0.5f) * 2.0f;
        float noiseY = ((float)((hash * 1237u) % 1000u) / 1000.0f - 0.5f) * 2.0f;
        vel.x += noiseX * params.turbulence * params.dt;
        vel.y += noiseY * params.turbulence * params.dt;
    }

    // Wobble (sinusoidal horizontal drift — for bubbles)
    if (params.wobbleFrequency > 0.0f && params.wobbleAmplitude > 0.0f) {
        // Each particle gets a phase offset based on its index
        float phase = (float)(idx * 73856093u % 10000u) / 10000.0f * 6.2831853f;
        float wobble = sinf(params.totalTime * params.wobbleFrequency * 6.2831853f + phase);
        vel.x += wobble * params.wobbleAmplitude * params.dt;
    }

    // --- Move ---
    pos.x += vel.x * params.dt;
    pos.y += vel.y * params.dt;

    // --- Tile collision ---
    if (params.collisionMasks && params.tileSize > 0.0f) {
        int tx = (int)floorf(pos.x / params.tileSize) - params.collisionOriginX;
        int ty = (int)floorf(pos.y / params.tileSize) - params.collisionOriginY;

        if (tx >= 0 && ty >= 0 && tx < params.collisionWidth && ty < params.collisionHeight) {
            if (params.collisionMasks[ty * params.collisionWidth + tx] != 0) {
                if (params.dieOnCollision) {
                    life = 0.0f; // kill it
                }
                else if (params.bounciness > 0.0f) {
                    // Revert position
                    pos.x = pv.x;
                    pos.y = pv.y;

                    // Axis-separated bounce (same technique as boids)
                    float2 tryX = make_float2(pv.x + vel.x * params.dt, pv.y);
                    int ttx = (int)floorf(tryX.x / params.tileSize) - params.collisionOriginX;
                    int tty = (int)floorf(tryX.y / params.tileSize) - params.collisionOriginY;
                    bool xSolid = false;
                    if (ttx >= 0 && tty >= 0 && ttx < params.collisionWidth && tty < params.collisionHeight)
                        xSolid = (params.collisionMasks[tty * params.collisionWidth + ttx] != 0);

                    float2 tryY = make_float2(pv.x, pv.y + vel.y * params.dt);
                    ttx = (int)floorf(tryY.x / params.tileSize) - params.collisionOriginX;
                    tty = (int)floorf(tryY.y / params.tileSize) - params.collisionOriginY;
                    bool ySolid = false;
                    if (ttx >= 0 && tty >= 0 && ttx < params.collisionWidth && tty < params.collisionHeight)
                        ySolid = (params.collisionMasks[tty * params.collisionWidth + ttx] != 0);

                    if (xSolid) vel.x *= -params.bounciness;
                    if (ySolid) vel.y *= -params.bounciness;
                    if (!xSolid && !ySolid) {
                        // Diagonal hit — bounce both
                        vel.x *= -params.bounciness;
                        vel.y *= -params.bounciness;
                    }
                }
                else {
                    // No bounce, no die — just stop
                    pos.x = pv.x;
                    pos.y = pv.y;
                    vel.x = 0.0f;
                    vel.y = 0.0f;
                }
            }
        }
    }

    // --- Kill out of bounds ---
    if (params.killOutOfBounds) {
        if (pos.x < params.boundsMinX || pos.x > params.boundsMaxX ||
            pos.y < params.boundsMinY || pos.y > params.boundsMaxY) {
            life = 0.0f;
        }
    }

    // --- Interpolate color and size over life ---
    float t = 1.0f; // default: fully "end" if dead
    if (maxLife > 0.0f && life > 0.0f) {
        t = 1.0f - (life / maxLife); // 0 = just born, 1 = about to die
    }

    float colorR = params.colorStartR + (params.colorEndR - params.colorStartR) * t;
    float colorG = params.colorStartG + (params.colorEndG - params.colorStartG) * t;
    float colorB = params.colorStartB + (params.colorEndB - params.colorStartB) * t;
    float colorA = params.colorStartA + (params.colorEndA - params.colorStartA) * t;
    float size = params.sizeStart + (params.sizeEnd - params.sizeStart) * t;

    // --- Rotation ---
    float rotation = sr.w;
    // Derive deterministic rotation speed per particle
    float rotSpeed = params.rotationSpeedMin +
        ((float)((idx * 5381u) % 1000u) / 1000.0f) *
        (params.rotationSpeedMax - params.rotationSpeedMin);
    if ((idx * 7919u) % 2u == 1u) rotSpeed = -rotSpeed;
    rotation += rotSpeed * params.dt;

    // --- Write back ---
    posVel[idx] = make_float4(pos.x, pos.y, vel.x, vel.y);
    lifeColor[idx] = make_float4(life, maxLife, colorR, colorG);
    sizeRot[idx] = make_float4(colorB, colorA, size, rotation);
}

// ============================================================
// Compact kernels
// ============================================================

// Step 1: Mark alive (1) or dead (0)
__global__ void markAliveKernel(const float4* __restrict__ lifeColor,
    int* __restrict__ alive,
    int count)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;
    alive[idx] = (lifeColor[idx].x > 0.0f) ? 1 : 0;
}

// Step 2: Scatter alive particles to their compacted positions
__global__ void scatterKernel(const float4* __restrict__ srcPV,
    const float4* __restrict__ srcLC,
    const float4* __restrict__ srcSR,
    float4* __restrict__ dstPV,
    float4* __restrict__ dstLC,
    float4* __restrict__ dstSR,
    const int* __restrict__ aliveFlags,
    const int* __restrict__ offsets,
    int count)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    if (aliveFlags[idx]) {
        if (srcLC[idx].x > 0.0f) {
            int dest = offsets[idx];
            dstPV[dest] = srcPV[idx];
            dstLC[dest] = srcLC[idx];
            dstSR[dest] = srcSR[idx];
        }
    }
}

// ============================================================
// Interleave kernel — pack SoA into interleaved VBO
// ============================================================
__global__ void interleaveKernel(float4* __restrict__ dst,
    const float4* __restrict__ posVel,
    const float4* __restrict__ lifeColor,
    const float4* __restrict__ sizeRot,
    int count)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    // Interleaved layout: [posVel0][lifeColor0][sizeRot0][posVel1][lifeColor1]...
    int base = idx * 3;
    dst[base + 0] = posVel[idx];
    dst[base + 1] = lifeColor[idx];
    dst[base + 2] = sizeRot[idx];
}

// ============================================================
// Host-side launch wrappers
// ============================================================
namespace CudaParticles {

    void Update(float4* posVel,
        float4* lifeColor,
        float4* sizeRot,
        const ParticleParams& params)
    {
        if (params.aliveCount <= 0) return;

        int threads = 256;
        int blocks = (params.aliveCount + threads - 1) / threads;

        updateKernel << <blocks, threads >> > (posVel, lifeColor, sizeRot, params);

#ifndef NDEBUG
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess)
            printf("[CudaParticles] Update kernel error: %s\n", cudaGetErrorString(err));
#endif
    }

    int Emit(float4* posVel,
        float4* lifeColor,
        float4* sizeRot,
        const ParticleParams& params)
    {
        int toEmit = params.emitCount + params.burstCount;
        if (toEmit <= 0) return params.aliveCount;

        // Clamp to buffer capacity
        int baseIndex = params.aliveCount;
        if (baseIndex + toEmit > params.maxParticles)
            toEmit = params.maxParticles - baseIndex;

        if (toEmit <= 0) return params.aliveCount;

        int threads = 256;
        int blocks = (toEmit + threads - 1) / threads;

        // Temporarily set emitCount to total (emit + burst)
        ParticleParams emitParams = params;
        emitParams.emitCount = toEmit;

        emitKernel << <blocks, threads >> > (posVel, lifeColor, sizeRot, emitParams, baseIndex);

#ifndef NDEBUG
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess)
            printf("[CudaParticles] Emit kernel error: %s\n", cudaGetErrorString(err));
#endif

        return baseIndex + toEmit;
    }

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
        size_t scanTempBytes)
    {
        (void)maxParticles;

        if (aliveCount <= 0) return 0;

        int threads = 256;
        int blocks = (aliveCount + threads - 1) / threads;

        // Step 1: mark alive
        markAliveKernel << <blocks, threads >> > (
            lifeColor,
            d_aliveFlags,
            aliveCount);

        // Step 2: exclusive scan using CUB
        cub::DeviceScan::ExclusiveSum(
            d_scanTemp,
            scanTempBytes,
            d_aliveFlags,
            d_offsets,
            aliveCount);

        // Step 3: compute totalAlive on GPU
        computeTotalAliveKernel << <1, 1 >> > (d_offsets, d_aliveFlags, d_totalAlive, aliveCount);

        // Copy back ONE int
        int totalAlive = 0;
        cudaMemcpy(&totalAlive, d_totalAlive, sizeof(int), cudaMemcpyDeviceToHost);

        // Step 4: scatter into temp buffers
        scatterKernel << <blocks, threads >> > (
            posVel, lifeColor, sizeRot,
            d_tempPV, d_tempLC, d_tempSR,
            d_aliveFlags,
            d_offsets,
            aliveCount);

        // Step 5: copy compacted back
        if (totalAlive > 0)
        {
            cudaMemcpy(posVel,
                d_tempPV,
                sizeof(float4) * totalAlive,
                cudaMemcpyDeviceToDevice);

            cudaMemcpy(lifeColor,
                d_tempLC,
                sizeof(float4) * totalAlive,
                cudaMemcpyDeviceToDevice);

            cudaMemcpy(sizeRot,
                d_tempSR,
                sizeof(float4) * totalAlive,
                cudaMemcpyDeviceToDevice);
        }

        return totalAlive;
    }

    void Interleave(float4* dst,
        const float4* posVel,
        const float4* lifeColor,
        const float4* sizeRot,
        int count)
    {
        if (count <= 0) return;

        int threads = 256;
        int blocks = (count + threads - 1) / threads;

        interleaveKernel << <blocks, threads >> > (dst, posVel, lifeColor, sizeRot, count);

#ifndef NDEBUG
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess)
            printf("[CudaParticles] Interleave kernel error: %s\n", cudaGetErrorString(err));
#endif
    }

    size_t GetScanTempBytes(int count, int* d_in, int* d_out)
    {
        size_t bytes = 0;
        cub::DeviceScan::ExclusiveSum(nullptr, bytes, d_in, d_out, count);
        return bytes;
    }

} // namespace CudaParticles

#endif // GRAPE_HAS_CUDA
