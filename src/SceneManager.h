#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <string>
#include "Scene.h"

// TODO: Implement scene transitions (fade in/out, etc.)
// and loading progress
class SceneManager final {
public:
    void AddScene(Scene* scene);
    void LoadScene(const std::string& name);
    void UnloadScene(const std::string& name);
    void RemoveScene(const std::string& name);
    void RemoveAllScenes();

    Scene* GetActiveScene() const;
    size_t GetSceneCount() const;
private:
    std::unordered_map<std::string, std::unique_ptr<Scene>> m_scenes;
    Scene* m_activeScene = nullptr;
};

#endif
