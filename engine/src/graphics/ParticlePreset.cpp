/* Start Header *****************************************************************/
/*!
\file     ParticlePreset.cpp
\author   Choi Meng Yew (100%)
\date     28th February 2026
\brief
Factory method implementations for particle presets.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "graphics/ParticlePreset.hpp"

ParticlePreset ParticlePreset::Bubbles() {
    ParticlePreset p;
    p.lifetimeMin = 2.0f;
    p.lifetimeMax = 5.0f;
    p.speedMin = 0.5f;        // was 20
    p.speedMax = 1.5f;        // was 60
    p.gravityY = 0.3f;        // was 15
    p.drag = 0.3f;
    p.wobbleFrequency = 1.5f;
    p.wobbleAmplitude = 0.5f; // was 25
    p.sizeStart = 0.2f;       // was 2
    p.sizeEnd = 0.5f;         // was 6
    p.colorStartA = 0.6f;
    p.colorEndA = 0.0f;
    p.emissionAngle = 1.5708f;
    p.emissionSpread = 0.5f;
    p.emissionShape = 0;
    return p;
}

ParticlePreset ParticlePreset::Geyser() {
    ParticlePreset p;
    p.lifetimeMin = 1.0f;
    p.lifetimeMax = 3.0f;
    p.speedMin = 3.0f;        // was 150
    p.speedMax = 6.0f;        // was 300
    p.gravityY = -1.5f;       // was 80 — negative = pulls down (check your Y convention)
    p.drag = 0.1f;
    p.turbulence = 0.8f;      // was 40
    p.sizeStart = 0.2f;       // was 3
    p.sizeEnd = 0.5f;         // was 8
    p.colorStartR = 0.8f; p.colorStartG = 0.9f; p.colorStartB = 1.0f; p.colorStartA = 0.9f;
    p.colorEndR = 0.6f; p.colorEndG = 0.7f; p.colorEndB = 0.9f; p.colorEndA = 0.0f;
    p.emissionAngle = 1.5708f;
    p.emissionSpread = 0.15f;
    p.emissionShape = 2;
    return p;
}

ParticlePreset ParticlePreset::Smoke() {
    ParticlePreset p;
    p.lifetimeMin = 3.0f;
    p.lifetimeMax = 8.0f;
    p.speedMin = 0.2f;        // was 10
    p.speedMax = 0.6f;        // was 30
    p.gravityY = 0.1f;        // was 5
    p.drag = 0.5f;
    p.turbulence = 0.3f;      // was 15
    p.sizeStart = 0.3f;       // was 5
    p.sizeEnd = 1.0f;         // was 25
    p.colorStartR = 0.4f; p.colorStartG = 0.4f; p.colorStartB = 0.4f; p.colorStartA = 0.5f;
    p.colorEndR = 0.2f; p.colorEndG = 0.2f; p.colorEndB = 0.2f; p.colorEndA = 0.0f;
    p.emissionAngle = 1.5708f;
    p.emissionSpread = 0.8f;
    p.emissionRadius = 0.5f;  // was 10
    p.emissionShape = 1;
    return p;
}

ParticlePreset ParticlePreset::Explosion() {
    ParticlePreset p;
    p.lifetimeMin = 0.3f;
    p.lifetimeMax = 1.5f;
    p.speedMin = 3.0f;        // was 100
    p.speedMax = 10.0f;       // was 400
    p.gravityY = 1.0f;        // was 50
    p.drag = 1.0f;
    p.sizeStart = 0.4f;       // was 6
    p.sizeEnd = 0.05f;        // was 1
    p.colorStartR = 1.0f; p.colorStartG = 0.8f; p.colorStartB = 0.3f; p.colorStartA = 1.0f;
    p.colorEndR = 0.8f; p.colorEndG = 0.2f; p.colorEndB = 0.1f; p.colorEndA = 0.0f;
    p.emissionSpread = 3.14159f;
    p.emissionShape = 0;
    return p;
}

ParticlePreset ParticlePreset::Sediment() {
    ParticlePreset p;
    p.lifetimeMin = 2.0f;
    p.lifetimeMax = 6.0f;
    p.speedMin = 0.1f;        // was 5
    p.speedMax = 0.3f;        // was 15
    p.gravityY = 0.2f;        // was 10
    p.drag = 0.8f;
    p.sizeStart = 0.1f;       // was 1
    p.sizeEnd = 0.1f;
    p.colorStartR = 0.6f; p.colorStartG = 0.5f; p.colorStartB = 0.3f; p.colorStartA = 0.4f;
    p.colorEndA = 0.0f;
    p.emissionAngle = 1.5708f;
    p.emissionSpread = 0.3f;
    p.emissionRadius = 1.0f;  // was 50 — now spans 1 tile
    p.emissionShape = 3;
    return p;
}