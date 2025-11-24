#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <nlohmann/json.hpp>
#include "EntitySerializer.h"
#include "Serializer.h"

using json = nlohmann::json;

/**
 * @brief Configuration structure for application settings
 * 
 * This structure holds all the configuration data that can be loaded
 * from the config.json file to customize application behavior.
 */
struct ApplicationConfig {
    /**
     * @brief Window-specific configuration settings
     */
    struct Window {
        int Width = 1600;           // Window width in pixels
        int Height = 900;           // Window height in pixels
        bool Fullscreen = false;    // Whether to start in fullscreen mode
        bool Vsync = true;          // Whether to enable vertical sync
    } WindowConfig;

    std::string Title = "GrapeEngine";  // Application window title
};

namespace Serialization {
    /**
     * @brief Handles loading, saving, and validating application configuration files
     * 
     * This class provides functionality to load and save application settings from
     * JSON configuration files, with fallback to default values if the file cannot
     * be read or parsed. It also provides basic validation.
     */
    class ConfigurationSerializer {
    public:
        /**
         * @brief Loads application configuration from a JSON file
         * @param configPath Path to the configuration JSON file
         * @param config Reference to ApplicationConfig structure to populate
         * @return true if config was loaded successfully, false if using defaults
         */
        static bool LoadConfig(const std::string& configPath, ApplicationConfig& config) {
            json configJson;
            if (!Serializer::LoadJson(configPath, "json", configJson)) {
                LOG_WARNING("Warning: Could not open config file: " << configPath);
                LOG_WARNING("Using default configuration.");
                config = GetDefaultConfig();
                SaveConfig(configPath, config); // Save default config
                return false;
            }

            if (configJson.contains("application")) {
                const auto& app = configJson["application"];
                if (app.contains("title")) {
                    config.Title = app["title"].get<std::string>();
                }
                if (app.contains("window")) {
                    _parseWindowConfig(app["window"], config.WindowConfig);
                }
            }

            LOG_DEBUG("Configuration loaded successfully from: " << configPath << '\n');
            return true;
        }

        /**
         * @brief Saves application configuration to a JSON file
         * @param configPath Path to the configuration JSON file
         * @param config Reference to ApplicationConfig structure to save
         * @return true if config was saved successfully, false otherwise
         */
        static bool SaveConfig(const std::string& configPath, const ApplicationConfig& config) {
            json configJson;
            configJson["application"]["title"] = config.Title;
            configJson["application"]["window"]["width"] = config.WindowConfig.Width;
            configJson["application"]["window"]["height"] = config.WindowConfig.Height;
            configJson["application"]["window"]["fullscreen"] = config.WindowConfig.Fullscreen;
            configJson["application"]["window"]["vsync"] = config.WindowConfig.Vsync;

            return Serializer::SaveJson(configPath, "json", configJson);
        }

        /**
         * @brief Validates the configuration structure
         * @param config Reference to ApplicationConfig structure to validate
         * @return true if valid, false otherwise
         */
        static bool ValidateConfig(const ApplicationConfig& config) {
            // Basic validation: width/height positive, title non-empty
            if (config.WindowConfig.Width <= 0 || config.WindowConfig.Height <= 0)
                return false;
            if (config.Title.empty())
                return false;
            return true;
        }

        /**
         * @brief Returns a default application configuration
         * @return ApplicationConfig with default values
         */
        static ApplicationConfig GetDefaultConfig() {
            return ApplicationConfig{};
        }
        
    private:
        static void _parseWindowConfig(const json& configJson, ApplicationConfig::Window& window) {
            if (configJson.contains("width")) {
                window.Width = configJson["width"].get<int>();
            }
            if (configJson.contains("height")) {
                window.Height = configJson["height"].get<int>();
            }
            if (configJson.contains("fullscreen")) {
                window.Fullscreen = configJson["fullscreen"].get<bool>();
            }
            if (configJson.contains("vsync")) {
                window.Vsync = configJson["vsync"].get<bool>();
            }
        }
    };
}

#endif