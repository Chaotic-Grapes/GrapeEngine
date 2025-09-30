#ifndef OVERLAY_H
#define OVERLAY_H
#include "ecs/ISystem.h"
#include "Audio.h"
#include "Application.h"

// Overlay inherits from Engine::ISystem
class Overlay final : public Engine::ISystem {
public:
    void OnCreate() override;  // One-time initialization
    void OnUpdate() override;  // ImGUI initialization, input processing, UI rendering
#ifdef USE_IMGUI
    ~Overlay() override;
#endif
    std::string Name() const override { return "Overlay"; }  // Name of system as a string
    void SetAudio(Systems::Audio* a) { m_audio = a; }
private: 
    Systems::Audio* m_audio = nullptr;
};

#endif
