#include "cuda/CudaBoids.cuh"

#ifdef GRAPE_HAS_CUDA

#include <curand_kernel.h>
#include <cstdio>

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
    float separationRange = params.visualRange * 0.4f;  // separation acts at closer range
    float separationRangeSq = separationRange * separationRange;

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

    // Clamp speed
    float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
    if (speed > params.maxSpeed) {
        vel.x = (vel.x / speed) * params.maxSpeed;
        vel.y = (vel.y / speed) * params.maxSpeed;
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
                    float maxSpeed) {
        if (count <= 0) return;

        int threads = 256;
        int blocks = (count + threads - 1) / threads;

        // Use time-based seed for variety
        unsigned long long seed = 42ull; // deterministic for debugging; use clock() for variety

        initRandomKernel<<<blocks, threads>>>(d_posVel, count, minX, minY, maxX, maxY, maxSpeed, seed);
        cudaDeviceSynchronize();
    }

} // namespace CudaBoids

#endif // GRAPE_HAS_CUDA
