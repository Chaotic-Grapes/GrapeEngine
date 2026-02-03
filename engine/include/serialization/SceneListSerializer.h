#ifndef SCENELIST_SERIALIZER_H
#define SCENELIST_SERIALIZER_H

#include <nlohmann/json.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include "core/Logger.h"
#include "serialization/Serializer.h"
#include "core/ProjectPaths.h"

using json = nlohmann::json;

namespace Serialization {
    class SceneListSerializer {
    public:
        static bool LoadSceneList(const std::string& listPath, std::vector<std::string>& scenesOut) {
            if (!std::filesystem::exists(listPath)) {
                return false;
            }

            json listJson;
            if (!Serializer::LoadJson(listPath, "json", listJson)) {
                LOG_WARNING("Failed to load SceneList.json: " << listPath);
                return false;
            }

            scenesOut.clear();
            if (listJson.is_array()) {
                _parseSceneArray(listJson, scenesOut);
            } else if (listJson.contains("Scenes") && listJson["Scenes"].is_array()) {
                _parseSceneArray(listJson["Scenes"], scenesOut);
            } else {
                LOG_WARNING("SceneList.json format invalid: " << listPath);
                return false;
            }

            return true;
        }

        static bool SaveSceneList(const std::string& listPath, const std::vector<std::string>& scenes) {
            json listJson = json::array();
            for (const auto& scene : scenes) {
                listJson.push_back(scene);
            }

            return Serializer::SaveJson(listPath, "json", listJson);
        }

        static bool BuildSceneListFromFolder(const std::string& scenesRoot, std::vector<std::string>& scenesOut) {
            scenesOut.clear();

            std::error_code ec;
            if (!std::filesystem::exists(scenesRoot, ec) || !std::filesystem::is_directory(scenesRoot, ec)) {
                LOG_WARNING("Scenes directory not found: " << scenesRoot);
                return false;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(scenesRoot)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                std::filesystem::path path = entry.path();
                std::string extension = path.extension().string();
                if (extension != ".scn" && extension != ".scene") {
                    continue;
                }

                std::string relativePath = Engine::ProjectPaths::ToRelativePath(path.string());
                if (relativePath.empty()) {
                    continue;
                }

                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                scenesOut.push_back(relativePath);
            }

            std::sort(scenesOut.begin(), scenesOut.end());
            return !scenesOut.empty();
        }

    private:
        static void _parseSceneArray(const json& array, std::vector<std::string>& scenesOut) {
            for (const auto& entry : array) {
                if (entry.is_string()) {
                    scenesOut.push_back(entry.get<std::string>());
                }
            }
        }
    };
}

#endif
