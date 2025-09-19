#ifndef OVERLAY_H
#define OVERLAY_H
#include "ecs/ISystem.h"

// Overlay inherits from Engine::ISystem
class Overlay : public Engine::ISystem {
public:
    void OnCreate() override;  // One-time initialization
    void OnUpdate() override;  // ImGUI initialization, input processing, UI rendering
    ~Overlay() override;
    std::string Name() const override { return "Overlay"; }  // Name of system as a string
};

#endif
