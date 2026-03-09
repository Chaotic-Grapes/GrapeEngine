/* Start Header *****************************************************************/
/*!
\file   CudaBoids.cu
\author Choi Meng Yew
\date   03/03/2026 DD/MM/YYYY
\brief
Implements the CUDA-accelerated Boid flock simulation for GrapeEngine.

This module contains GPU kernels and host-side launch wrappers for
updating large-scale boid simulations entirely on the GPU. Each thread
simulates one boid using a double-buffered position+velocity layout.

Features:
    - Brute-force neighbor evaluation (O(n²))
    - Separation, Alignment, Cohesion steering rules
    - Per-boid jitter for natural spacing variation
    - Predictive tile-based collision avoidance
    - Hard collision resolution with wall sliding
    - Periodic speed burst behavior for organic motion
    - World-space wraparound bounds
    - GPU-based random initialization using cuRAND

Responsibilities:
    - Compute steering forces per boid
    - Integrate velocity and position
    - Enforce force and speed constraints
    - Handle tile collision avoidance and resolution
    - Write updated state to output buffer
    - Provide host launch wrappers for engine systems

Used by:
    - BoidSystem (GPU simulation path)
    - CudaGLInterop (for mapped VBO access)
    - Editor-controlled BoidParams

This module forms the compute core of the GPU Boid pipeline.
*/
/* End Header *******************************************************************/

#include "cuda/CudaBoids.cuh"

#ifdef GRAPE_HAS_CUDA

#include <curand_kernel.h>
#include <cstdio>

/*!
\brief
Device-side helper that computes tile-based collision avoidance steering.

This is a __device__ function, meaning it runs on the GPU and may only be
called from other CUDA kernels or device functions. It executes once per
boid thread and contributes a steering vector used inside the main
simulation kernel.

The function combines:

    - Proximity-based repulsion from nearby solid tiles
    - Predictive lookahead sampling along velocity direction
    - Perpendicular steering when a wall is detected ahead

Returns a steering vector that pushes the boid away from obstacles.
No memory is written; the result is accumulated into velocity by
the calling kernel.

\param pos    Current world-space position of the boid
\param vel    Current velocity of the boid
\param p      BoidParams containing tile grid and collision data
\return       Avoidance steering force (float2)
*/
__device__ float2 ComputeCollisionAvoidance(float2 pos, float2 vel, const BoidParams& p)
{
    float2 steer = make_float2(0.0f, 0.0f);
    if (!p.collisionMasks || p.tileSize <= 0.0f) return steer;

    float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
    if (speed < 1e-4f) return steer;

    float avoidRadius = p.tileSize * p.collisionAvoidRadius;

    // 1) Proximity repulsion (existing approach, kept)
    int tx = (int)floorf(pos.x / p.tileSize) - p.collisionOriginX;
    int ty = (int)floorf(pos.y / p.tileSize) - p.collisionOriginY;
    int searchRadius = (int)ceilf(p.collisionAvoidRadius);

    for (int dy = -searchRadius; dy <= searchRadius; dy++) {
        for (int dx = -searchRadius; dx <= searchRadius; dx++) {
            int nx = tx + dx;
            int ny = ty + dy;
            if (nx < 0 || ny < 0 || nx >= p.collisionWidth || ny >= p.collisionHeight) continue;
            if (p.collisionMasks[ny * p.collisionWidth + nx] == 0) continue;

            float tcx = ((nx + p.collisionOriginX) + 0.5f) * p.tileSize;
            float tcy = ((ny + p.collisionOriginY) + 0.5f) * p.tileSize;

            float2 away = make_float2(pos.x - tcx, pos.y - tcy);
            float dist = sqrtf(away.x * away.x + away.y * away.y);
            if (dist < 1e-4f || dist > avoidRadius) continue;

            float weight = 1.0f - (dist / avoidRadius);
            weight *= weight; // quadratic falloff — stronger close up
            steer.x += (away.x / dist) * weight;
            steer.y += (away.y / dist) * weight;
        }
    }

    // 2) Predictive: sample ahead along velocity direction
    float2 dir = make_float2(vel.x / speed, vel.y / speed);
    float lookahead = fminf(speed * p.dt * 3.0f, avoidRadius); // look 3 frames ahead

    for (float t = p.tileSize * 0.5f; t <= lookahead; t += p.tileSize * 0.5f) {
        float sx = pos.x + dir.x * t;
        float sy = pos.y + dir.y * t;

        int stx = (int)floorf(sx / p.tileSize) - p.collisionOriginX;
        int sty = (int)floorf(sy / p.tileSize) - p.collisionOriginY;

        if (stx < 0 || sty < 0 || stx >= p.collisionWidth || sty >= p.collisionHeight) continue;
        if (p.collisionMasks[sty * p.collisionWidth + stx] == 0) continue;

        // Wall ahead! Steer perpendicular to velocity (choose side away from wall)
        float2 perp1 = make_float2(-dir.y, dir.x);
        float2 perp2 = make_float2(dir.y, -dir.x);

        // Pick the perpendicular that points away from the wall center
        float tcx = ((stx + p.collisionOriginX) + 0.5f) * p.tileSize;
        float tcy = ((sty + p.collisionOriginY) + 0.5f) * p.tileSize;
        float dot1 = (pos.x - tcx) * perp1.x + (pos.y - tcy) * perp1.y;

        float2 chosen = (dot1 >= 0.0f) ? perp1 : perp2;
        float urgency = 1.0f - (t / lookahead); // more urgent when closer
        urgency *= urgency;

        steer.x += chosen.x * urgency * 2.0f;
        steer.y += chosen.y * urgency * 2.0f;
        break; // first hit is most important
    }

    return steer;
}

// ============================================================
// Boids simulation kernel (brute force)
// Each thread = one boid. Reads all other boids from prev buffer,
// computes three rules, writes updated pos+vel to output buffer.
// ============================================================
__global__ void boidsKernel(float4* __restrict__ posVel,
                            const float4* __restrict__ posVelPrev,
                            BoidParams params) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.count) return;

    // Read this boid's previous state
    float4 self = posVelPrev[idx];
    float2 pos  = make_float2(self.x, self.y);
    float2 vel  = make_float2(self.z, self.w);

    // Accumulators for the three rules
    float2 separation = make_float2(0.0f, 0.0f);
    float2 alignment  = make_float2(0.0f, 0.0f);
    float2 cohesion   = make_float2(0.0f, 0.0f);

    int neighborCount = 0;
    float visualRangeSq = params.visualRange * params.visualRange;
    float jitter = 0.3f + 0.4f * ((idx * 2654435761u) % 1000u) / 1000.0f; // 0.3 to 0.7, unique per boid
    float separationRange = params.visualRange * jitter;
    float separationRangeSq = separationRange * separationRange;

    // After computing separation, before applying weights, add in boidsKernel:
    float noiseSeed = (float)((idx * 1234567u + (unsigned int)(params.dt * 1000)) % 1000u) / 1000.0f;
    separation.x += (noiseSeed - 0.5f) * 0.3f;
    separation.y += (noiseSeed - 0.5f) * 0.3f;

    // Brute force: check all other boids
    for (int j = 0; j < params.count; ++j) {
        if (j == idx) continue;

        float4 other = posVelPrev[j];
        float2 otherPos = make_float2(other.x, other.y);
        float2 otherVel = make_float2(other.z, other.w);

        float2 diff = make_float2(pos.x - otherPos.x, pos.y - otherPos.y);
        float distSq = diff.x * diff.x + diff.y * diff.y;

        // Within visual range?
        if (distSq < visualRangeSq && distSq > 0.0001f) {
            // Alignment: average velocity of neighbors
            alignment.x += otherVel.x;
            alignment.y += otherVel.y;

            // Cohesion: average position of neighbors
            cohesion.x += otherPos.x;
            cohesion.y += otherPos.y;

            neighborCount++;

            // Separation: steer away from very close boids
            if (distSq < separationRangeSq) {
                float dist = sqrtf(distSq);
                separation.x += diff.x / dist;
                separation.y += diff.y / dist;
            }
        }
    }

    // Compute steering forces
    float2 force = make_float2(0.0f, 0.0f);

    if (neighborCount > 0) {
        float invCount = 1.0f / (float)neighborCount;

        // Alignment: steer towards average heading
        alignment.x *= invCount;
        alignment.y *= invCount;
        // Desired = alignment direction * maxSpeed - current velocity
        float alignLen = sqrtf(alignment.x * alignment.x + alignment.y * alignment.y);
        if (alignLen > 0.001f) {
            alignment.x = (alignment.x / alignLen) * params.maxSpeed - vel.x;
            alignment.y = (alignment.y / alignLen) * params.maxSpeed - vel.y;
        }

        // Cohesion: steer towards average position
        cohesion.x = cohesion.x * invCount - pos.x;
        cohesion.y = cohesion.y * invCount - pos.y;
        float cohLen = sqrtf(cohesion.x * cohesion.x + cohesion.y * cohesion.y);
        if (cohLen > 0.001f) {
            cohesion.x = (cohesion.x / cohLen) * params.maxSpeed - vel.x;
            cohesion.y = (cohesion.y / cohLen) * params.maxSpeed - vel.y;
        }

        // Apply weighted forces
        force.x += separation.x * params.separationWeight;
        force.y += separation.y * params.separationWeight;
        force.x += alignment.x * params.alignmentWeight;
        force.y += alignment.y * params.alignmentWeight;
        force.x += cohesion.x * params.cohesionWeight;
        force.y += cohesion.y * params.cohesionWeight;
    }

    // Clamp force
    float forceMag = sqrtf(force.x * force.x + force.y * force.y);
    if (forceMag > params.maxForce) {
        force.x = (force.x / forceMag) * params.maxForce;
        force.y = (force.y / forceMag) * params.maxForce;
    }

    // Integrate
    vel.x += force.x * params.dt;
    vel.y += force.y * params.dt;

    // Speed burst: occasional individual bursts
    unsigned int burstHash = (idx * 2654435761u) ^ (params.frameCount * 2246822519u);
    unsigned int cycle = 200u + (idx * 7919u) % 100u;       // 200-300 frame period, varies per boid
    unsigned int phase = burstHash % cycle;
    unsigned int burstDuration = 15u + (idx * 1237u) % 10u;  // 15-25 frames

    float burstMultiplier = 1.0f;
    if (phase < burstDuration) {
        float t = (float)phase / (float)burstDuration;
        float envelope = (t < 0.3f) ? (t / 0.3f) : (1.0f - (t - 0.3f) / 0.7f);
        burstMultiplier = 1.0f + envelope * 0.8f; // up to 1.8x speed

        float spd = sqrtf(vel.x * vel.x + vel.y * vel.y);
        if (spd > 0.001f) {
            float impulse = params.maxSpeed * envelope * 0.5f;
            vel.x += (vel.x / spd) * impulse * params.dt;
            vel.y += (vel.y / spd) * impulse * params.dt;
        }
    }

    // Tile collision avoidance - applied directly to vel, bypasses maxForce
    if (params.collisionAvoidWeight > 0.0f) {
        float2 avoid = ComputeCollisionAvoidance(pos, vel, params);
        vel.x += avoid.x * params.collisionAvoidWeight * params.dt;
        vel.y += avoid.y * params.collisionAvoidWeight * params.dt;

        float avoidSpeed = sqrtf(vel.x * vel.x + vel.y * vel.y);
        if (avoidSpeed > params.maxSpeed * 1.5f) {
            vel.x = (vel.x / avoidSpeed) * params.maxSpeed * 1.5f;
            vel.y = (vel.y / avoidSpeed) * params.maxSpeed * 1.5f;
        }
    }

    // Clamp speed (with burst allowance)
    float effectiveMaxSpeed = params.maxSpeed * burstMultiplier;
    float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
    if (speed > effectiveMaxSpeed) {
        vel.x = (vel.x / speed) * effectiveMaxSpeed;
        vel.y = (vel.y / speed) * effectiveMaxSpeed;
    }

    // Enforce minimum speed so boids don't stall
    float minSpeed = params.maxSpeed * 0.3f;
    if (speed < minSpeed && speed > 0.001f) {
        vel.x = (vel.x / speed) * minSpeed;
        vel.y = (vel.y / speed) * minSpeed;
    }

    // Update position
    pos.x += vel.x * params.dt;
    pos.y += vel.y * params.dt;

    // Hard collision resolution — don't let boids exist inside walls
    if (params.collisionMasks && params.tileSize > 0.0f) {
        int tx = (int)floorf(pos.x / params.tileSize) - params.collisionOriginX;
        int ty = (int)floorf(pos.y / params.tileSize) - params.collisionOriginY;

        if (tx >= 0 && ty >= 0 && tx < params.collisionWidth && ty < params.collisionHeight) {
            if (params.collisionMasks[ty * params.collisionWidth + tx] != 0) {
                // Step 1: Revert position
                pos.x = self.x;
                pos.y = self.y;

                // Step 2: Slide along wall instead of reflecting
                // Try moving only on X axis
                float2 tryPos = make_float2(self.x + vel.x * params.dt, self.y);
                int ttx = (int)floorf(tryPos.x / params.tileSize) - params.collisionOriginX;
                int tty = (int)floorf(tryPos.y / params.tileSize) - params.collisionOriginY;
                bool xOk = (ttx >= 0 && tty >= 0 && ttx < params.collisionWidth && tty < params.collisionHeight)
                    ? (params.collisionMasks[tty * params.collisionWidth + ttx] == 0) : true;

                // Try moving only on Y axis
                tryPos = make_float2(self.x, self.y + vel.y * params.dt);
                ttx = (int)floorf(tryPos.x / params.tileSize) - params.collisionOriginX;
                tty = (int)floorf(tryPos.y / params.tileSize) - params.collisionOriginY;
                bool yOk = (ttx >= 0 && tty >= 0 && ttx < params.collisionWidth && tty < params.collisionHeight)
                    ? (params.collisionMasks[tty * params.collisionWidth + ttx] == 0) : true;

                if (xOk && !yOk) {
                    // Slide along X, zero Y velocity
                    pos.x = self.x + vel.x * params.dt;
                    vel.y = 0.0f;
                }
                else if (yOk && !xOk) {
                    // Slide along Y, zero X velocity
                    pos.y = self.y + vel.y * params.dt;
                    vel.x = 0.0f;
                }
                else if (xOk && yOk) {
                    // Both axes free — pick the one with more velocity
                    if (fabsf(vel.x) > fabsf(vel.y)) {
                        pos.x = self.x + vel.x * params.dt;
                        vel.y *= -0.3f; // dampen, slight bounce
                    }
                    else {
                        pos.y = self.y + vel.y * params.dt;
                        vel.x *= -0.3f;
                    }
                }
                else {
                    // Stuck in a corner — stay put, dampen velocity
                    vel.x *= -0.1f;
                    vel.y *= -0.1f;
                }

                // Re-clamp speed
                float s = sqrtf(vel.x * vel.x + vel.y * vel.y);
                if (s > params.maxSpeed) {
                    vel.x = (vel.x / s) * params.maxSpeed;
                    vel.y = (vel.y / s) * params.maxSpeed;
                }
            }
        }
    }

    // Wrap around bounds
    float w = params.boundsMaxX - params.boundsMinX;
    float h = params.boundsMaxY - params.boundsMinY;
    if (pos.x < params.boundsMinX) pos.x += w;
    if (pos.x > params.boundsMaxX) pos.x -= w;
    if (pos.y < params.boundsMinY) pos.y += h;
    if (pos.y > params.boundsMaxY) pos.y -= h;

    // Write output
    posVel[idx] = make_float4(pos.x, pos.y, vel.x, vel.y);
}

// ============================================================
// Random initialization kernel
// ============================================================
__global__ void initRandomKernel(float4* posVel,
                                  int count,
                                  float minX, float minY,
                                  float maxX, float maxY,
                                  float maxSpeed,
                                  unsigned long long seed) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    curandState state;
    curand_init(seed, idx, 0, &state);

    float px = minX + curand_uniform(&state) * (maxX - minX);
    float py = minY + curand_uniform(&state) * (maxY - minY);

    // Random direction with random speed
    float angle = curand_uniform(&state) * 6.2831853f;  // 2*PI
    float speed = maxSpeed * (0.3f + 0.7f * curand_uniform(&state));
    float vx = cosf(angle) * speed;
    float vy = sinf(angle) * speed;

    posVel[idx] = make_float4(px, py, vx, vy);
}

// ============================================================
// Host-side launch wrappers
// ============================================================
namespace CudaBoids {

    void Launch(float4* d_posVel,
                const float4* d_posVelPrev,
                const BoidParams& params) {
        if (params.count <= 0) return;

        int threads = 256;
        int blocks = (params.count + threads - 1) / threads;

        boidsKernel<<<blocks, threads>>>(d_posVel, d_posVelPrev, params);

        // Check for errors (debug builds)
#ifndef NDEBUG
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            printf("[CudaBoids] Kernel launch error: %s\n", cudaGetErrorString(err));
        }
#endif
    }

    void InitRandom(float4* d_posVel,
                    int count,
                    float minX, float minY,
                    float maxX, float maxY,
                    float maxSpeed,
                    uint32_t entitySeed) {
        if (count <= 0) return;

        int threads = 256;
        int blocks = (count + threads - 1) / threads;

        // Mix entity index into seed so different flocks diverge
        unsigned long long seed = 42ull ^ ((unsigned long long)entitySeed * 2654435761ull);

        initRandomKernel<<<blocks, threads>>>(d_posVel, count, minX, minY, maxX, maxY, maxSpeed, seed);
        cudaDeviceSynchronize();
    }

} // namespace CudaBoids

#endif // GRAPE_HAS_CUDA
