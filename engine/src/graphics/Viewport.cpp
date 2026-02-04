// Viewport.cpp
#include "graphics/Viewport.hpp"
#include "ecs/systems/RendererSystem.h"

namespace Graphics {

    void ViewportManager::Create(const std::string& name, Engine::Camera* camera, int w, int h) {
        if (auto* r = ECS::RendererSystem::GetInstance())
            r->AddViewport(name, camera, w, h);
    }

    void ViewportManager::Destroy(const std::string& name) {
        if (auto* r = ECS::RendererSystem::GetInstance())
            r->RemoveViewport(name);
    }

    void ViewportManager::Resize(const std::string& name, int w, int h) {
        if (auto* r = ECS::RendererSystem::GetInstance())
            r->ResizeViewport(name, w, h);
    }

    void ViewportManager::SetCamera(const std::string& name, Engine::Camera* camera) {
        if (auto* r = ECS::RendererSystem::GetInstance())
            r->SetViewportCamera(name, camera);
    }

    uint32_t ViewportManager::GetTexture(const std::string& name) {
        if (auto* r = ECS::RendererSystem::GetInstance())
            return r->GetViewportTexture(name);
        return 0;
    }

    uint32_t ViewportManager::Pick(const std::string& name, float localX, float localY) {
        auto* r = ECS::RendererSystem::GetInstance();
        if (!r) return UINT32_MAX;

        auto* vp = r->GetViewport(name);
        if (!vp) return UINT32_MAX;

        return r->RequestPick(localX, localY, { 0, 0 }, glm::vec2(vp->Size));
    }

    bool ViewportManager::GetPickResult(uint32_t requestId, uint32_t& outEntityId) {
        if (auto* r = ECS::RendererSystem::GetInstance())
            return r->TryGetPickResult(requestId, outEntityId);
        return false;
    }

    glm::vec2 ViewportManager::ScreenToWorld(const std::string& name, float localX, float localY) {
        auto* r = ECS::RendererSystem::GetInstance();
        if (!r) return { 0, 0 };

        auto* vp = r->GetViewport(name);
        if (!vp || !vp->Camera) return { 0, 0 };

        glm::vec4 ndc{
            (2.0f * localX) / vp->Size.x - 1.0f,
            1.0f - (2.0f * localY) / vp->Size.y,
            0.0f, 1.0f
        };

        glm::mat4 invVP = glm::inverse(vp->Camera->GetProjectionMatrix() * vp->Camera->GetViewMatrix());
        glm::vec4 world = invVP * ndc;
        return { world.x, world.y };
    }

} // namespace Graphics