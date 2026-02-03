#ifndef SCENELIST_LOADER_H
#define SCENELIST_LOADER_H

#include <string>
#include <vector>
#include <filesystem>
#include "scene/SceneManager.h"
#include "serialization/SceneListSerializer.h"
#include "core/ProjectPaths.h"
#include "core/Logger.h"

namespace Scenes {
    class SceneListLoader {
    public:
        static bool LoadFromFile(SceneManager& manager, const std::string& listPath, bool keepActive = true) {
            std::vector<std::string> sceneList;
            if (!Serialization::SceneListSerializer::LoadSceneList(listPath, sceneList)) {
                if (!Serialization::SceneListSerializer::BuildSceneListFromFolder(Engine::ProjectPaths::GetScenesPath(), sceneList)) {
                    return false;
                }

                if (!Serialization::SceneListSerializer::SaveSceneList(listPath, sceneList)) {
                    LOG_WARNING("Failed to save generated SceneList.json: " << listPath);
                }
            }

            if (sceneList.empty()) {
                LOG_WARNING("SceneList.json contains no scene entries.");
                return false;
            }

            const size_t sceneCount = manager.GetSceneCount();
            const size_t activeIndex = manager.GetActiveIndex();
            const bool hasActive = activeIndex != static_cast<size_t>(-1);

            if (!sceneList.empty() && hasActive) {
                const Scene* activeScene = manager.GetScene(activeIndex);
                if (activeScene) {
                    const std::string activePath = _normalizePath(_resolveScenePath(activeScene->GetPath()));
                    bool foundActive = false;
                    for (size_t i = 0; i < sceneList.size(); ++i) {
                        const std::string entryPath = _normalizePath(_resolveScenePath(sceneList[i]));
                        if (entryPath == activePath) {
                            foundActive = true;
                            break;
                        }
                    }

                    if (!foundActive && !activePath.empty()) {
                        sceneList.push_back(activeScene->GetPath());
                        LOG_INFO("SceneListLoader: appended active scene to list order.");
                    }
                }
            }

            size_t firstLoadedIndex = static_cast<size_t>(-1);
            for (const auto& entry : sceneList) {
                if (entry.empty()) {
                    continue;
                }

                std::string absolutePath = _resolveScenePath(entry);

                if (_isSceneLoaded(manager, absolutePath)) {
                    continue;
                }

                auto* scene = new Scene();
                size_t sceneIndex = manager.AddScene(scene);
                if (!manager.LoadScene(sceneIndex, absolutePath)) {
                    LOG_WARNING("Failed to load scene from list: " << absolutePath);
                    manager.RemoveScene(sceneIndex);
                    continue;
                }

                if (firstLoadedIndex == static_cast<size_t>(-1)) {
                    firstLoadedIndex = sceneIndex;
                }
            }

            if (!keepActive) {
                if (firstLoadedIndex != static_cast<size_t>(-1)) {
                    manager.SetActive(firstLoadedIndex);
                }
            } else if (!hasActive && firstLoadedIndex != static_cast<size_t>(-1)) {
                manager.SetActive(firstLoadedIndex);
            }

            return true;
        }

        static bool LoadFromDefault(SceneManager& manager, bool keepActive = true) {
            return LoadFromFile(manager, Engine::ProjectPaths::GetSceneListPath(), keepActive);
        }

    private:
        static bool _isSceneLoaded(const SceneManager& manager, const std::string& absolutePath) {
            const std::string normalizedTarget = _normalizePath(absolutePath);
            const size_t count = manager.GetSceneCount();

            for (size_t i = 0; i < count; ++i) {
                const Scene* scene = manager.GetScene(i);
                if (!scene) {
                    continue;
                }

                std::string scenePath = scene->GetPath();
                if (scenePath.empty()) {
                    continue;
                }

                std::filesystem::path scenePathFs(scenePath);
                std::string sceneAbsolute = scenePathFs.is_absolute()
                    ? scenePathFs.string()
                    : Engine::ProjectPaths::ToAbsolutePath(scenePath);

                if (_normalizePath(sceneAbsolute) == normalizedTarget) {
                    return true;
                }
            }

            return false;
        }

        static std::string _resolveScenePath(const std::string& path) {
            if (path.empty()) {
                return {};
            }

            std::filesystem::path pathFs(path);
            std::string absolutePath = pathFs.is_absolute()
                ? pathFs.string()
                : Engine::ProjectPaths::ToAbsolutePath(path);

            return absolutePath;
        }

        static std::string _normalizePath(const std::string& path) {
            std::filesystem::path normalized = std::filesystem::path(path).lexically_normal();
            std::string normalizedStr = normalized.string();
            std::replace(normalizedStr.begin(), normalizedStr.end(), '\\', '/');
            return normalizedStr;
        }
    };
}

#endif
