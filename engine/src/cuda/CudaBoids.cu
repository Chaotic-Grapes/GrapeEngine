/* Start Header *****************************************************************/
/*!
\file   CudaBoids.cu
\author Choi Meng Yew (100%)
\date   03/03/2026 DD/MM/YYYY
\brief
Implements the CUDA-accelerated Boid flock simulation for GrapeEngine.

This module contains GPU kernels and host-side launch wrappers for
updating large-scale boid simulations entirely on the GPU. Each thread
simulates one boid using a double-buffered position+velocity layout.

Features:
    - Spatial hashing for O(1) neighbor queries
    - Separation, Alignment, Cohesion steering rules
    - Per-boid jitter for natural spacing variation
    - Predictive tile-based collision avoidance
    - Hard collision resolution with wall sliding
    - Periodic speed burst behavior for organic motion
    - World-space wraparound bounds
    - GPU-based random initialization using cuRAND

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "cuda/CudaBoids.cuh"

#ifdef GRAPE_HAS_CUDA

#include <curand_kernel.h>
#include <thrust/device_ptr.h>
#include <thrust/sort.h>
#include <cstdio>

/**
 * @brief Computes tile-based collision avoidance steering for a boid.
 *
 * Combines proximity-based repulsion from nearby solid tiles, predictive
 * lookahead sampling along the velocity direction, and perpendicular
 * steering when a wall is detected ahead.
 *
 * @param pos Current world-space position of the boid.
 * @param vel Current velocity of the boid.
 * @param p   Boid simulation parameters including collision mask and tile data.
 * @return Steering force vector that pushes the boid away from obstacles.
 */
__device__ float2 ComputeCollisionAvoidance(float2 pos, float2 vel, const BoidParams& p)
{
    float2 steer = make_float2(0.0f, 0.0f);
    if (!p.collisionMasks || p.tileSize <= 0.0f) return steer;

    float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
    if (speed < 1e-4f) return steer;

    float avoidRadius = p.tileSize * p.collisionAvoidRadius;

    // 1) Proximity repulsion
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
            weight *= weight; // quadratic falloff
            steer.x += (away.x / dist) * weight;
            steer.y += (away.y / dist) * weight;
        }
    }

    // 2) Predictive: sample ahead along velocity direction
    float2 dir = make_float2(vel.x / speed, vel.y / speed);
    float lookahead = fminf(speed * p.dt * 3.0f, avoidRadius);

    for (float t = p.tileSize * 0.5f; t <= lookahead; t += p.tileSize * 0.5f) {
        float sx = pos.x + dir.x * t;
        float sy = pos.y + dir.y * t;

        int stx = (int)floorf(sx / p.tileSize) - p.collisionOriginX;
        int sty = (int)floorf(sy / p.tileSize) - p.collisionOriginY;

        if (stx < 0 || sty < 0 || stx >= p.collisionWidth || sty >= p.collisionHeight) continue;
        if (p.collisionMasks[sty * p.collisionWidth + stx] == 0) continue;

        // Wall ahead! Steer perpendicular to velocity
        float2 perp1 = make_float2(-dir.y, dir.x);
        float2 perp2 = make_float2(dir.y, -dir.x);

        float tcx = ((stx + p.collisionOriginX) + 0.5f) * p.tileSize;
        float tcy = ((sty + p.collisionOriginY) + 0.5f) * p.tileSize;
        float dot1 = (pos.x - tcx) * perp1.x + (pos.y - tcy) * perp1.y;

        float2 chosen = (dot1 >= 0.0f) ? perp1 : perp2;
        float urgency = 1.0f - (t / lookahead);
        urgency *= urgency;

        steer.x += chosen.x * urgency * 2.0f;
        steer.y += chosen.y * urgency * 2.0f;
        break; // first hit is most important
    }

    return steer;
}

// ============================================================
// Spatial hash helper
// ============================================================

/**
 * @brief Computes a spatial hash index for a 2D grid cell.
 *
 * Maps a (cx, cy) integer cell coordinate to a bucket index in the hash
 * table using a pair of large prime multipliers.
 *
 * @param cx        Integer X coordinate of the grid cell.
 * @param cy        Integer Y coordinate of the grid cell.
 * @param tableSize Total number of buckets in the hash table.
 * @return Hash bucket index in the range [0, tableSize).
 */
__device__ __forceinline__ uint32_t HashCell(int cx, int cy, int tableSize) {
    uint32_t h = ((uint32_t)cx * 92837111u) ^ ((uint32_t)cy * 689287499u);
    return h % (uint32_t)tableSize;
}

// ============================================================
// Kernel 1: assign each boid to a hash cell
// ============================================================

/**
 * @brief CUDA kernel that assigns each boid to a spatial hash cell.
 *
 * Each thread reads one boid's position from @p posVel, computes the
 * grid cell it occupies, hashes that cell, and writes the resulting cell
 * ID and the boid's own index into the output arrays for subsequent
 * sorting and neighbour lookup.
 *
 * @param posVel        Device array of boid position+velocity (float4: x, y, vx, vy).
 * @param d_cellIds     Output array that receives the hashed cell ID for each boid.
 * @param d_boidIds     Output array that receives the boid index for each boid.
 * @param cellSize      World-space size of a single hash cell.
 * @param hashTableSize Total number of buckets in the hash table.
 * @param count         Total number of boids to process.
 */
__global__ void assignCellsKernel(
    const float4* __restrict__ posVel,
    uint32_t* __restrict__ d_cellIds,
    uint32_t* __restrict__ d_boidIds,
    float cellSize,
    int hashTableSize,
    int count)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    float4 p = posVel[idx];
    int cx = (int)floorf(p.x / cellSize);
    int cy = (int)floorf(p.y / cellSize);

    d_cellIds[idx] = HashCell(cx, cy, hashTableSize);
    d_boidIds[idx] = (uint32_t)idx;
}

// ============================================================
// Kernel 2: build cell start table
// ============================================================

/**
 * @brief CUDA kernel that builds the cell-start lookup table for spatial hashing.
 *
 * After the (cellId, boidId) pairs have been sorted by cell ID, each thread
 * checks whether it is the first entry for its cell and, if so, records its
 * index as the start position for that cell in @p d_cellStart.
 *
 * @param d_cellIds   Device array of sorted cell IDs (one per boid).
 * @param d_cellStart Output table; entry [c] holds the first sorted index that
 *                    belongs to cell c, or 0xFFFFFFFF if the cell is empty.
 * @param count       Total number of boids (length of @p d_cellIds).
 */
__global__ void buildCellStartKernel(
    const uint32_t* __restrict__ d_cellIds,
    uint32_t* __restrict__ d_cellStart,
    int count)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    uint32_t cell = d_cellIds[idx];

    if (idx == 0 || d_cellIds[idx - 1] != cell) {
        d_cellStart[cell] = (uint32_t)idx;
    }
}

// ============================================================
// Boids simulation kernel
// ============================================================

/**
 * @brief CUDA kernel that advances the boid flocking simulation by one time step.
 *
 * Each thread simulates one boid. Using the spatially-hashed neighbour
 * tables built by @p assignCellsKernel and @p buildCellStartKernel, each
 * thread queries a 3x3 neighbourhood of hash cells to accumulate
 * separation, alignment, and cohesion steering forces. The kernel also
 * applies per-boid jitter, speed burst behaviour, tile collision avoidance
 * via ComputeCollisionAvoidance(), hard collision resolution with wall
 * sliding, and world-space wraparound bounds.
 *
 * @param posVel     Output device array of updated boid position+velocity
 *                   (float4: x, y, vx, vy).
 * @param posVelPrev Input device array of previous-frame boid state
 *                   (float4: x, y, vx, vy), read-only.
 * @param params     Boid simulation parameters (counts, weights, bounds,
 *                   hash tables, collision data, etc.).
 */
__global__ void boidsKernel(float4* __restrict__ posVel,
    const float4* __restrict__ posVelPrev,
    BoidParams params) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= params.count) return;

    // Read this boid's previous state
    float4 self = posVelPrev[idx];
    float2 pos = make_float2(self.x, self.y);
    float2 vel = make_float2(self.z, self.w);

    // Accumulators for the three rules
    float2 separation = make_float2(0.0f, 0.0f);
    float2 alignment = make_float2(0.0f, 0.0f);
    float2 cohesion = make_float2(0.0f, 0.0f);

    int neighborCount = 0;
    float visualRangeSq = params.visualRange * params.visualRange;
    float jitter = 0.3f + 0.4f * ((idx * 2654435761u) % 1000u) / 1000.0f;
    float separationRange = params.visualRange * jitter;
    float separationRangeSq = separationRange * separationRange;

    float noiseSeed = (float)((idx * 1234567u + (unsigned int)(params.dt * 1000)) % 1000u) / 1000.0f;
    separation.x += (noiseSeed - 0.5f) * 0.3f;
    separation.y += (noiseSeed - 0.5f) * 0.3f;

    // Spatial hash neighbor search
    int cx = (int)floorf(pos.x / params.cellSize);
    int cy = (int)floorf(pos.y / params.cellSize);

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            uint32_t cell = HashCell(cx + dx, cy + dy, params.hashTableSize);
            uint32_t start = params.d_cellStart[cell];
            if (start == 0xFFFFFFFFu) continue;

            for (uint32_t s = start; s < (uint32_t)params.count; ++s) {
                if (params.d_cellIds[s] != cell) break;

                int j = (int)params.d_boidIds[s];
                if (j == idx) continue;

                float4 other = posVelPrev[j];
                float2 otherPos = make_float2(other.x, other.y);
                float2 otherVel = make_float2(other.z, other.w);

                float2 diff = make_float2(pos.x - otherPos.x, pos.y - otherPos.y);
                float distSq = diff.x * diff.x + diff.y * diff.y;

                if (distSq < visualRangeSq && distSq > 0.0001f) {
                    alignment.x += otherVel.x;
                    alignment.y += otherVel.y;
                    cohesion.x += otherPos.x;
                    cohesion.y += otherPos.y;
                    neighborCount++;

                    if (distSq < separationRangeSq) {
                        float dist = sqrtf(distSq);
                        separation.x += diff.x / dist;
                        separation.y += diff.y / dist;
                    }
                }
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
    unsigned int cycle = 200u + (idx * 7919u) % 100u;
    unsigned int phase = burstHash % cycle;
    unsigned int burstDuration = 15u + (idx * 1237u) % 10u;

    float burstMultiplier = 1.0f;
    if (phase < burstDuration) {
        float t = (float)phase / (float)burstDuration;
        float envelope = (t < 0.3f) ? (t / 0.3f) : (1.0f - (t - 0.3f) / 0.7f);
        burstMultiplier = 1.0f + envelope * 0.8f;

        float spd = sqrtf(vel.x * vel.x + vel.y * vel.y);
        if (spd > 0.001f) {
            float impulse = params.maxSpeed * envelope * 0.5f;
            vel.x += (vel.x / spd) * impulse * params.dt;
            vel.y += (vel.y / spd) * impulse * params.dt;
        }
    }

    // Tile collision avoidance
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

    // Enforce minimum speed
    float minSpeed = params.maxSpeed * 0.3f;
    if (speed < minSpeed && speed > 0.001f) {
        vel.x = (vel.x / speed) * minSpeed;
        vel.y = (vel.y / speed) * minSpeed;
    }

    // Update position
    pos.x += vel.x * params.dt;
    pos.y += vel.y * params.dt;

    // Hard collision resolution
    if (params.collisionMasks && params.tileSize > 0.0f) {
        int tx = (int)floorf(pos.x / params.tileSize) - params.collisionOriginX;
        int ty = (int)floorf(pos.y / params.tileSize) - params.collisionOriginY;

        if (tx >= 0 && ty >= 0 && tx < params.collisionWidth && ty < params.collisionHeight) {
            if (params.collisionMasks[ty * params.collisionWidth + tx] != 0) {
                pos.x = self.x;
                pos.y = self.y;

                float2 tryPos = make_float2(self.x + vel.x * params.dt, self.y);
                int ttx = (int)floorf(tryPos.x / params.tileSize) - params.collisionOriginX;
                int tty = (int)floorf(tryPos.y / params.tileSize) - params.collisionOriginY;
                bool xOk = (ttx >= 0 && tty >= 0 && ttx < params.collisionWidth && tty < params.collisionHeight)
                    ? (params.collisionMasks[tty * params.collisionWidth + ttx] == 0) : true;

                tryPos = make_float2(self.x, self.y + vel.y * params.dt);
                ttx = (int)floorf(tryPos.x / params.tileSize) - params.collisionOriginX;
                tty = (int)floorf(tryPos.y / params.tileSize) - params.collisionOriginY;
                bool yOk = (ttx >= 0 && tty >= 0 && ttx < params.collisionWidth && tty < params.collisionHeight)
                    ? (params.collisionMasks[tty * params.collisionWidth + ttx] == 0) : true;

                if (xOk && !yOk) {
                    pos.x = self.x + vel.x * params.dt;
                    vel.y = 0.0f;
                }
                else if (yOk && !xOk) {
                    pos.y = self.y + vel.y * params.dt;
                    vel.x = 0.0f;
                }
                else if (xOk && yOk) {
                    if (fabsf(vel.x) > fabsf(vel.y)) {
                        pos.x = self.x + vel.x * params.dt;
                        vel.y *= -0.3f;
                    }
                    else {
                        pos.y = self.y + vel.y * params.dt;
                        vel.x *= -0.3f;
                    }
                }
                else {
                    vel.x *= -0.1f;
                    vel.y *= -0.1f;
                }

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

/**
 * @brief CUDA kernel that randomly initialises boid positions and velocities.
 *
 * Each thread uses cuRAND to place one boid at a random position within
 * the supplied world-space rectangle, retrying up to 32 times to avoid
 * spawning inside a solid tile.  A random heading and speed in
 * [0.3, 1.0] * maxSpeed are also assigned.
 *
 * @param posVel            Output device array (float4: x, y, vx, vy).
 * @param count             Total number of boids to initialise.
 * @param minX              Minimum X bound of the spawn area.
 * @param minY              Minimum Y bound of the spawn area.
 * @param maxX              Maximum X bound of the spawn area.
 * @param maxY              Maximum Y bound of the spawn area.
 * @param maxSpeed          Maximum initial speed assigned to each boid.
 * @param seed              Base cuRAND seed; combined with the boid index.
 * @param collisionMasks    Optional device pointer to solid-tile bitmask array,
 *                          or nullptr to skip collision checks.
 * @param collisionWidth    Width of the collision mask grid in tiles.
 * @param collisionHeight   Height of the collision mask grid in tiles.
 * @param collisionOriginX  X origin offset of the collision grid in tiles.
 * @param collisionOriginY  Y origin offset of the collision grid in tiles.
 * @param tileSize          World-space size of one tile in pixels/units.
 */
__global__ void initRandomKernel(float4* posVel,
    int count,
    float minX, float minY,
    float maxX, float maxY,
    float maxSpeed,
    unsigned long long seed,
    const uint8_t* collisionMasks,
    int collisionWidth,
    int collisionHeight,
    int collisionOriginX,
    int collisionOriginY,
    float tileSize)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    curandState state;
    curand_init(seed, idx, 0, &state);

    float px, py;
    int attempts = 0;
    do {
        px = minX + curand_uniform(&state) * (maxX - minX);
        py = minY + curand_uniform(&state) * (maxY - minY);

        if (!collisionMasks || tileSize <= 0.0f) break;

        int tx = (int)floorf(px / tileSize) - collisionOriginX;
        int ty = (int)floorf(py / tileSize) - collisionOriginY;

        bool inBounds = tx >= 0 && ty >= 0 && tx < collisionWidth && ty < collisionHeight;
        if (!inBounds || collisionMasks[ty * collisionWidth + tx] == 0) break;

        attempts++;
    } while (attempts < 32);

    float angle = curand_uniform(&state) * 6.2831853f;
    float speed = maxSpeed * (0.3f + 0.7f * curand_uniform(&state));
    float vx = cosf(angle) * speed;
    float vy = sinf(angle) * speed;

    posVel[idx] = make_float4(px, py, vx, vy);
}

// ============================================================
// Host-side launch wrappers
// ============================================================
namespace CudaBoids {

    /**
     * @brief Launches the full boid simulation pipeline for one frame.
     *
     * Executes five sequential steps on the given CUDA stream:
     * 1. assignCellsKernel  — map boids to spatial hash cells.
     * 2. thrust::sort_by_key — sort (cellId, boidId) pairs.
     * 3. cudaMemsetAsync     — reset the cell-start table.
     * 4. buildCellStartKernel — populate the cell-start lookup table.
     * 5. boidsKernel         — run separation/alignment/cohesion and integrate.
     *
     * @param d_posVel     Output device buffer for updated boid state (float4 per boid).
     * @param d_posVelPrev Input device buffer containing previous-frame boid state.
     * @param params       Boid simulation parameters, including pre-allocated hash
     *                     table device pointers and all tuning weights.
     * @param stream       CUDA stream on which all work is enqueued.
     */
    void Launch(float4* d_posVel,
        const float4* d_posVelPrev,
        const BoidParams& params,
        cudaStream_t stream) {
        if (params.count <= 0) return;

        int threads = 256;
        int blocks = (params.count + threads - 1) / threads;

        // Step 1: assign boids to hash cells
        assignCellsKernel << <blocks, threads, 0, stream >> > (
            d_posVelPrev,
            params.d_cellIds,
            params.d_boidIds,
            params.cellSize,
            params.hashTableSize,
            params.count);

        // Step 2: sort boid ids by cell id
        thrust::device_ptr<uint32_t> keys(params.d_cellIds);
        thrust::device_ptr<uint32_t> vals(params.d_boidIds);
        thrust::sort_by_key(thrust::cuda::par.on(stream), keys, keys + params.count, vals);

        // Step 3: clear cell start table
        cudaMemsetAsync(params.d_cellStart, 0xFF,
            sizeof(uint32_t) * params.hashTableSize, stream);

        // Step 4: build cell start table
        buildCellStartKernel << <blocks, threads, 0, stream >> > (
            params.d_cellIds,
            params.d_cellStart,
            params.count);

        // Step 5: run boid simulation with spatial lookup
        boidsKernel << <blocks, threads, 0, stream >> > (d_posVel, d_posVelPrev, params);

#ifndef NDEBUG
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            printf("[CudaBoids] Kernel launch error: %s\n", cudaGetErrorString(err));
        }
#endif
    }

    /**
     * @brief Randomly initialises boid positions and velocities on the device.
     *
     * Launches initRandomKernel with a per-entity seed derived from
     * @p entitySeed, then synchronises the default stream so that the caller
     * can immediately use the populated buffer.  Boids are placed within the
     * rectangle [@p minX, @p maxX] x [@p minY, @p maxY] and will retry up to
     * 32 times to avoid spawning inside a solid tile when @p collisionMasks is
     * provided.
     *
     * @param d_posVel         Device buffer to receive initialised boid state
     *                         (float4: x, y, vx, vy) for @p count boids.
     * @param count            Number of boids to initialise.
     * @param minX             Minimum X bound of the spawn rectangle.
     * @param minY             Minimum Y bound of the spawn rectangle.
     * @param maxX             Maximum X bound of the spawn rectangle.
     * @param maxY             Maximum Y bound of the spawn rectangle.
     * @param maxSpeed         Maximum initial speed assigned to each boid.
     * @param entitySeed       Per-entity seed mixed into the cuRAND base seed.
     * @param collisionMasks   Optional device pointer to solid-tile bitmask; pass
     *                         nullptr to skip spawn collision checks.
     * @param collisionWidth   Width of the collision mask grid in tiles.
     * @param collisionHeight  Height of the collision mask grid in tiles.
     * @param collisionOriginX X origin offset of the collision grid in tiles.
     * @param collisionOriginY Y origin offset of the collision grid in tiles.
     * @param tileSize         World-space size of one tile in pixels/units.
     */
    void InitRandom(float4* d_posVel,
        int count,
        float minX, float minY,
        float maxX, float maxY,
        float maxSpeed,
        uint32_t entitySeed,
        const uint8_t* collisionMasks,
        int collisionWidth,
        int collisionHeight,
        int collisionOriginX,
        int collisionOriginY,
        float tileSize) {
        if (count <= 0) return;

        int threads = 256;
        int blocks = (count + threads - 1) / threads;

        unsigned long long seed = 42ull ^ ((unsigned long long)entitySeed * 2654435761ull);

        initRandomKernel << <blocks, threads >> > (
            d_posVel, count,
            minX, minY, maxX, maxY,
            maxSpeed, seed,
            collisionMasks,
            collisionWidth, collisionHeight,
            collisionOriginX, collisionOriginY,
            tileSize);

        // Only used at init time, safe to sync default stream here
        cudaDeviceSynchronize();
    }

} // namespace CudaBoids

#endif // GRAPE_HAS_CUDA
