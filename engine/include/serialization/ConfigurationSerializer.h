#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <vector>
#include "EntitySerializer.h"
#include "Serializer.h"

using json = nlohmann::json;

/**
 * @brief Project configuration structure
 * 
 * This structure holds game project settings loaded from ProjectSettings.json
 * Used by both engine runtime and editor.
 */
struct ProjectSettings {
    /**
     * @brief Window-specific configuration settings
     */
    struct Window {
        int Width = 1600;           // Window width in pixels
        int Height = 900;           // Window height in pixels
        bool Fullscreen = true;     // Whether to start in fullscreen mode
        bool VSync = true;          // Whether to enable vertical sync
        std::string Mode = "Fullscreen"; // Windowed, Fullscreen, Borderless (covers monitor).
    } WindowSettings;

    /**
     * @brief Physics-specific configuration settings
     */
    struct Physics {
        float Gravity = -9.81f;    // Gravity acceleration
        float TimeStep = 0.016f;   // Physics time step
    } Physics;

    // Root-level game metadata
    std::string Title = "GrapeEngine Game Project"; // Game/project name
    std::string Version = "1.0.0";                  // Version string
    std::string StartupScene = "";                  // Path to startup scene

    // Audio configuration
    float MasterVolume = 1.0f;       // Global volume multiplier [0,1]
    float MusicVolume  = 1.0f;       // Music volume multiplier [0,1]
    float SFXVolume    = 1.0f;       // Sound effects volume multiplier [0,1]
    bool  MuteWhenUnfocused = false; // Mute audio when window loses focus

    // Performance configuration
    int MaxFPS    = 120; // Hard cap when VSync is disabled
};

namespace Serialization {
    /**
     * @brief Handles loading, saving, and validating project configuration files
     * 
     * This class provides functionality to load and save project settings from
     * JSON configuration files. Used by both engine runtime and editor.
     * 
     * Note: Editor-specific settings are handled by Editor::EditorConfiguration.
     */
    class ConfigurationSerializer {
    public:
        /**
         * @brief Loads project settings from a JSON file
         * @param settingsPath Path to the ProjectSettings.json file
         * @param settings Reference to ProjectSettings structure to populate
         * @return true if settings were loaded successfully
         */
        static bool LoadProjectSettings(const std::string& settingsPath, ProjectSettings& settings) {
            json settingsJson;
            if (!Serializer::LoadJson(settingsPath, "json", settingsJson)) {
                LOG_WARNING("Warning: Could not open project settings file: " << settingsPath);
                return false;
            }

            // Parse root-level fields
            if (settingsJson.contains("Title")) {
                settings.Title = settingsJson["Title"].get<std::string>();
            }
            if (settingsJson.contains("Version")) {
                settings.Version = settingsJson["Version"].get<std::string>();
            }
            if (settingsJson.contains("StartupScene")) {
                settings.StartupScene = settingsJson["StartupScene"].get<std::string>();
            }

            // Parse WindowSettings
            if (settingsJson.contains("WindowSettings")) {
                _parseWindowConfig(settingsJson["WindowSettings"], settings.WindowSettings);
            }

            // Parse Physics
            if (settingsJson.contains("Physics")) {
                const auto& physics = settingsJson["Physics"];
                if (physics.contains("Gravity")) {
                    settings.Physics.Gravity = physics["Gravity"].get<float>();
                }
                if (physics.contains("TimeStep")) {
                    settings.Physics.TimeStep = physics["TimeStep"].get<float>();
                }
            }

            // Parse Audio
            if (settingsJson.contains("Audio") && settingsJson["Audio"].is_object()) {
                const auto& audio = settingsJson["Audio"];
                if (audio.contains("MasterVolume")) {
                    settings.MasterVolume = audio["MasterVolume"].get<float>();
                }
                if (audio.contains("MusicVolume")) {
                    settings.MusicVolume = audio["MusicVolume"].get<float>();
                }
                if (audio.contains("SFXVolume")) {
                    settings.SFXVolume = audio["SFXVolume"].get<float>();
                }
                if (audio.contains("MuteWhenUnfocused")) {
                    settings.MuteWhenUnfocused = audio["MuteWhenUnfocused"].get<bool>();
                }
            }

            // Parse Performance
            if (settingsJson.contains("Performance") && settingsJson["Performance"].is_object()) {
                const auto& perf = settingsJson["Performance"];
                if (perf.contains("MaxFPS")) {
                    settings.MaxFPS = perf["MaxFPS"].get<int>();
                }
            }

            LOG_DEBUG("Project settings loaded successfully from: " << settingsPath << '\n');
            return true;
        }

        /**
         * @brief Saves project settings to a JSON file
         * @param settingsPath Path to the ProjectSettings.json file
         * @param settings Reference to ProjectSettings structure to save
         * @return true if settings were saved successfully
         */
        static bool SaveProjectSettings(const std::string& settingsPath, const ProjectSettings& settings) {
            json settingsJson;
            settingsJson["Title"] = settings.Title;
            settingsJson["Version"] = settings.Version;
            settingsJson["StartupScene"] = settings.StartupScene;
            
            settingsJson["WindowSettings"]["Width"] = settings.WindowSettings.Width;
            settingsJson["WindowSettings"]["Height"] = settings.WindowSettings.Height;
            // Normalize to keep legacy Fullscreen in sync with Mode.
            const std::string mode = _normalizeWindowMode(settings.WindowSettings.Mode);
            settingsJson["WindowSettings"]["Mode"] = mode;
            settingsJson["WindowSettings"]["Fullscreen"] = (mode == "Fullscreen" || mode == "BorderlessFullscreen");
            settingsJson["WindowSettings"]["VSync"] = settings.WindowSettings.VSync;
            
            settingsJson["Physics"]["Gravity"] = settings.Physics.Gravity;
            settingsJson["Physics"]["TimeStep"] = settings.Physics.TimeStep;

            // Audio block
            settingsJson["Audio"]["MasterVolume"] = settings.MasterVolume;
            settingsJson["Audio"]["MusicVolume"]  = settings.MusicVolume;
            settingsJson["Audio"]["SFXVolume"]    = settings.SFXVolume;
            settingsJson["Audio"]["MuteWhenUnfocused"] = settings.MuteWhenUnfocused;

            // Performance block
            settingsJson["Performance"]["MaxFPS"]    = settings.MaxFPS;

            return Serializer::SaveJson(settingsPath, "json", settingsJson);
        }

        /**
         * @brief Validates the project settings structure
         * @param settings Reference to ProjectSettings structure to validate
         * @return true if valid, false otherwise
         */
        static bool ValidateProjectSettings(const ProjectSettings& settings) {
            // Basic validation: width/height positive, game name non-empty
            if (settings.WindowSettings.Width <= 0 || settings.WindowSettings.Height <= 0)
                return false;
            if (_normalizeWindowMode(settings.WindowSettings.Mode).empty())
                return false;
            if (settings.Title.empty())
                return false;
            return true;
        }
        
    private:
        static std::string _normalizeWindowMode(const std::string& mode) {
            if (mode.empty()) {
                return "Windowed";
            }

            std::string lowered = mode;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lowered == "fullscreen" || lowered == "exclusive") {
                return "Fullscreen";
            }
            if (lowered == "borderlessfullscreen" || lowered == "borderless_fullscreen" || lowered == "borderless fullscreen" ||
                lowered == "borderless") {
                return "Borderless";
            }
            if (lowered == "windowed" || lowered == "window") {
                return "Windowed";
            }

            return "Windowed";
        }

        static void _parseWindowConfig(const json& configJson, ProjectSettings::Window& window) {
            if (configJson.contains("Width")) {
                window.Width = configJson["Width"].get<int>();
            }
            if (configJson.contains("Height")) {
                window.Height = configJson["Height"].get<int>();
            }
            if (configJson.contains("Mode")) {
                // Prefer explicit Mode; fall back to legacy Fullscreen otherwise.
                window.Mode = _normalizeWindowMode(configJson["Mode"].get<std::string>());
                window.Fullscreen = (window.Mode == "Fullscreen");
            } else if (configJson.contains("Fullscreen")) {
                window.Fullscreen = configJson["Fullscreen"].get<bool>();
                window.Mode = window.Fullscreen ? "Fullscreen" : "Windowed";
            }
            if (configJson.contains("VSync")) {
                window.VSync = configJson["VSync"].get<bool>();
            }
        }
    };
}

#endif
