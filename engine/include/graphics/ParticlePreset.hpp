/* Start Header *****************************************************************/
/*!
\file     ParticlePreset.h
\author   Choi Meng Yew (100%)
\date     28th February 2026
\brief
Shared read-only configuration for particle emitters. Multiple emitter
entities can reference the same preset by ID. Presets are registered on
ParticleSystem at startup and looked up at runtime.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

struct ParticlePreset {
    // --- Lifetime ---
    float lifetimeMin = 1.0f;
    float lifetimeMax = 3.0f;

    // --- Speed ---
    float speedMin = 50.0f;
    float speedMax = 150.0f;

    // --- Physics ---
    float gravityX = 0.0f;
    float gravityY = 0.0f;
    float drag = 0.0f;
    float turbulence = 0.0f;

    // --- Wobble (bubbles) ---
    float wobbleFrequency = 0.0f;
    float wobbleAmplitude = 0.0f;

    // --- Size over life ---
    float sizeStart = 4.0f;
    float sizeEnd = 1.0f;

    // --- Rotation ---
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;

    // --- Color start (RGBA 0-1) ---
    float colorStartR = 1.0f;
    float colorStartG = 1.0f;
    float colorStartB = 1.0f;
    float colorStartA = 1.0f;

    // --- Color end (RGBA 0-1) ---
    float colorEndR = 1.0f;
    float colorEndG = 1.0f;
    float colorEndB = 1.0f;
    float colorEndA = 0.0f;

    // --- Emission shape ---
    float emissionAngle = -1.5708f; // radians (-PI/2 = upward)
    float emissionSpread = 0.3f;
    float emissionRadius = 0.0f;
    int   emissionShape = 0;        // 0=Point, 1=Circle, 2=Cone, 3=Line

    // --- Collision ---
    bool  dieOnCollision = false;
    bool  killOutOfBounds = true;
    float bounciness = 0.0f;

    // --- Factory presets ---

    /**
     * @brief Create a bubble particle preset with upward drift and wobble.
     * @return Pre-configured bubble emitter preset.
     */
    static ParticlePreset Bubbles();

    /**
     * @brief Create a geyser particle preset with high upward velocity.
     * @return Pre-configured geyser emitter preset.
     */
    static ParticlePreset Geyser();

    /**
     * @brief Create a smoke particle preset with slow rise and fade.
     * @return Pre-configured smoke emitter preset.
     */
    static ParticlePreset Smoke();

    /**
     * @brief Create an explosion particle preset with outward burst.
     * @return Pre-configured explosion emitter preset.
     */
    static ParticlePreset Explosion();

    /**
     * @brief Create a sediment particle preset with downward drift.
     * @return Pre-configured sediment emitter preset.
     */
    static ParticlePreset Sediment();
};