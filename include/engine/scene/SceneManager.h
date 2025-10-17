#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <memory>
#include <vector>
#include "scene/Scene.h"

namespace Scenes {
    class SceneManager {
    public:
        using ScenePtr = std::unique_ptr<Scene>;

        size_t AddScene(ScenePtr scene) {
            m_scenes.push_back(std::move(scene));
            if (m_active == NPOS)
                m_active = 0;

            return m_scenes.size() - 1;
        }
        void SetActive(size_t idx) {
            if (idx < m_scenes.size())
                m_active = idx;
        }
        Scene* GetActive() {
            if (m_active == NPOS) return nullptr;

            return m_scenes[m_active].get();
        }
        void Update(float dt) {
            if (auto* s = GetActive())
                s->Update(dt);
        }

    private:
        std::vector<ScenePtr> m_scenes;
        static constexpr size_t NPOS = size_t(-1);
        size_t m_active = NPOS;
    };
}

#endif
