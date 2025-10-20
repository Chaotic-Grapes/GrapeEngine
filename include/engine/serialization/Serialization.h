/**
 * @file    Serialization.h
 * @author  k.danielneozuofeng@digipen.edu
 * @date    26/09/2025
 * @brief   Scene serialization and deserialization to JSON
 *
 * This header defines the SceneSerializer class, which provides
 * static methods to save and load an entire World (all entities
 * and their components) to and from JSON files using the
 * nlohmann::json library and EntitySerializer utilities.
 */


#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <nlohmann/json.hpp>
#include "EntitySerializer.h"
#include <fstream>
#include <iostream>

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

// TODO: Merge with SceneManager
namespace Serialization {
	/**
	 * @brief Handles serialization and deserialization of game scenes to/from JSON files
	 * 
	 * This class provides static methods to save and load entire game scenes,
	 * including all entities and their components. Scenes are stored in JSON format
	 * for easy editing and version control.
	 */
	class SceneSerializer {
	public:
		/**
		 * @brief Saves a complete scene to a JSON file
		 * 
		 * Serializes all entities in the world along with their components
		 * and saves them to the specified file path in JSON format.
		 * 
		 * @param world Reference to the world containing entities to save
		 * @param filename Path to the output JSON file
		 * @return true if save was successful, false otherwise
		 */
		static bool SaveScene(World& world, const std::string& filename) {
			try {
				// Create the main scene JSON structure
				json scene;
				scene["Version"] = "1.0";
				scene["SceneName"] = "TestScene";
				scene["EntityCount"] = 0; // Will be updated below

				// Initialize entities array
				json entities = json::array();

				// Serialize each entity in the world
				int entityCount = 0;
				world.ForEachEntity([&](Entity entity) {
					entities.push_back(EntitySerializer::SerializeEntity(entity));
					entityCount++;
					});

				// Update scene data with serialized entities
				scene["Entities"] = entities;
				scene["EntityCount"] = entityCount;

				// Write JSON to file with pretty formatting
				std::ofstream file(filename);
				file << scene.dump(4);
				file.close();

				// Log success information
				std::cout << "Scene successfully saved to: " << filename << '\n';
				std::cout << " Entities: " << entityCount << '\n';
				return true;

			}
			catch (const std::exception& e) {
				std::cout << "Error saving scene: " << e.what() << '\n';
				return false;
			}
		}

		/**
		 * @brief Loads a complete scene from a JSON file
		 * 
		 * Deserializes all entities and their components from the specified
		 * JSON file and recreates them in the world. This will destroy all
		 * existing entities before loading the new scene.
		 * 
		 * @param world Reference to the world where entities will be created
		 * @param filename Path to the input JSON file
		 * @return true if load was successful, false otherwise
		 */
		static bool LoadScene(World& world, const std::string& filename) {
			try {
				// Open and validate the JSON file
				std::ifstream file(filename);
				if (!file.is_open()) {
					std::cout << " Cannot open file: " << filename << '\n';
					return false;
				}

				// Parse the JSON content
				json scene = json::parse(file);
				file.close();

				// Log scene information
				std::cout << "Scene successfully loaded: " << scene["SceneName"] << '\n';
				std::cout << " Version: " << scene["Version"] << '\n';

				// Clear existing entities before loading new scene
				world.GetEntityManager().DestroyAllEntities();

				// Deserialize each entity from the JSON array
				if (scene.contains("Entities")) {
					int loadedCount = 0;
					for (const auto& entityJson : scene["Entities"]) {
						EntitySerializer::DeserializeEntity(world, entityJson);
						loadedCount++;
					}
					std::cout << " Entities loaded: " << loadedCount << '\n';
				}
				return true;

			}
			catch (const json::parse_error& e) {
				std::cout << " JSON parse error: " << e.what() << '\n';
				return false;
			}
			catch (const std::exception& e) {
				std::cout << " Error loading scene: " << e.what() << '\n';
				return false;
			}
		}
	};

	/**
	 * @brief Handles loading and parsing of application configuration files
	 * 
	 * This class provides functionality to load application settings from
	 * JSON configuration files, with fallback to default values if the
	 * file cannot be read or parsed.
	 */
	class ConfigLoader {
	public:
		/**
		 * @brief Loads application configuration from a JSON file
		 * 
		 * Attempts to load and parse the configuration file. If the file
		 * cannot be opened or parsed, falls back to default configuration.
		 * 
		 * @param configPath Path to the configuration JSON file
		 * @param config Reference to ApplicationConfig structure to populate
		 * @return true if config was loaded successfully, false if using defaults
		 */
		static bool LoadConfig(const std::string& configPath, ApplicationConfig& config) {
			try {
				// Attempt to open the configuration file
				std::ifstream configFile(configPath);
				if (!configFile.is_open()) {
					std::cerr << "Warning: Could not open config file: " << configPath << std::endl;
					std::cerr << "Using default configuration." << std::endl;
					config = GetDefaultConfig();
					return false;
				}

				// Parse the JSON content
				json configJson;
				configFile >> configJson;

				// Parse application-level settings
				if (configJson.contains("application")) {
					const auto& app = configJson["application"];
					
					// Load application title if present
					if (app.contains("title")) {
						config.Title = app["title"].get<std::string>();
					}

					// Load window configuration if present
					if (app.contains("window")) {
						_ParseWindowConfig(app["window"], config.WindowConfig);
					}
				}

				std::cout << "Configuration loaded successfully from: " << configPath << "\n\n";
				return true;

			} catch (const std::exception& e) {
				std::cerr << "Error loading config file: " << e.what() << std::endl;
				std::cerr << "Using default configuration." << std::endl;
				config = GetDefaultConfig();
				return false;
			}
		}

		/**
		 * @brief Returns a default application configuration
		 * 
		 * Creates and returns an ApplicationConfig structure with
		 * sensible default values for all settings.
		 * 
		 * @return ApplicationConfig with default values
		 */
		static ApplicationConfig GetDefaultConfig() {
			return ApplicationConfig{};
		}
		
	private:
		/**
		 * @brief Parses window-specific configuration from JSON
		 * 
		 * Helper function to extract window settings from the configuration
		 * JSON and populate the Window structure with the values.
		 * 
		 * @param configJson JSON object containing window configuration
		 * @param window Reference to Window structure to populate
		 */
		static void _ParseWindowConfig(const json& configJson, ApplicationConfig::Window& window) {
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