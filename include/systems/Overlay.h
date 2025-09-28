#ifndef OVERLAY_H
#define OVERLAY_H
#include "ecs/ISystem.h"

// Forward declaration
class World;

// Overlay inherits from Engine::ISystem
class Overlay : public Engine::ISystem {
public:
    void OnCreate() override;  // One-time initialization
    void OnUpdate() override;  // ImGUI initialization, input processing, UI rendering

#ifdef USE_IMGUI
    ~Overlay() override;
#endif
    std::string Name() const override { return "Overlay"; }  // Name of system as a string

    // Setter method to provide world reference
    void SetWorld(World * world) { m_world = world; }

private:
    // Store world reference
    World* m_world = nullptr;
};

#endif
