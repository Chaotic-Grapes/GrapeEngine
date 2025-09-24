#include <unordered_map>
#include <memory>
#include <string>
#include "ecs/SceneManager.h"

Scene& SceneManager::CreateScene(const std::string& name) {
    auto scene = std::make_unique<Scene>(name);
    Scene* raw = scene.get();
    m_scenes[name] = std::move(scene);
    return *raw;
}

void SceneManager::LoadScene(const std::string& name) {
    if (m_activeScene) {
        m_activeScene->Unload();
    }

    const auto it = m_scenes.find(name);
    if (it != m_scenes.end()) {
        m_activeScene = it->second.get();
        m_activeScene->Load();
    }
}

void SceneManager::UnloadScene(const std::string& name) {
    const auto it = m_scenes.find(name);
    if (it != m_scenes.end() && it->second.get() == m_activeScene) {
        m_activeScene->Unload();
        m_activeScene = nullptr;
    }
}

void SceneManager::RemoveScene(const std::string& name) {
    const auto it = m_scenes.find(name);
    if (it != m_scenes.end()) {
        if (it->second.get() == m_activeScene) {
            m_activeScene->Unload();
            m_activeScene = nullptr;
        }
        m_scenes.erase(it);
    }
}

void SceneManager::ClearAllScenes() {
    if (m_activeScene) {
        m_activeScene->Unload();
        m_activeScene = nullptr;
    }
    m_scenes.clear();
}

Scene* SceneManager::GetActiveScene() const { return m_activeScene; }
size_t SceneManager::GetSceneCount()  const { return m_scenes.size(); }