/* Start Header *****************************************************************/
/*!
\file   AnimationAssetManager.cpp
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Implements animation asset loading and transient asset creation.
*/
/* End Header *******************************************************************/

#include "animation/AnimationAssetManager.h"
#include "core/Logger.h"
#include "serialization/Serializer.h"
#include "core/ProjectPaths.h"
#include <filesystem>
#include "math/Vector2D.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
    constexpr uint32_t kFNVOffsetBasis = 2166136261u;           // FNV-1a 32-bit offset basis
    constexpr uint32_t kFNVPrime = 16777619u;                   // FNV-1a 32-bit prime
    constexpr const char* kTransientPrefix = "transient://";    // Prefix for transient assets that are not loaded from disk

    /**
     * @brief Computes a FNV-1a hash of the given string, which is used for generating unique IDs for animation assets based on their paths.
     * @param value The input string to hash.
     * @return A 32-bit unsigned integer representing the hash of the input string.
     */
    uint32_t HashString(const std::string& value) {
        uint32_t hash = kFNVOffsetBasis;

        // FNV-1a hash algorithm: XOR each byte with the hash, then multiply by the prime
        for (char c : value) {
            hash ^= static_cast<uint8_t>(c);
            hash *= kFNVPrime;
        }

        return hash;
    }

    /**
     * @brief Parses a JSON array into a Vector2D structure, with error handling to return a fallback value if the JSON is not in the expected format.
     * @param j The JSON value to parse, expected to be an array of at least 2 elements representing the X and Y components of the vector.
     * @param fallback A Vector2D value to return if the JSON is not a valid array for parsing.
     * @return A Vector2D constructed from the JSON array, or the fallback value if parsing fails.
     */
    Vector2D ParseVector2D(const json& j, const Vector2D& fallback) {
        if (!j.is_array() || j.size() < 2) {
            return fallback;
        }

        return Vector2D{
            j.at(0).get<float>(),
            j.at(1).get<float>()
        };
    }

    /**
     * @brief Parses a JSON array into a Color structure, with error handling to return a fallback value if the JSON is not in the expected format.
     * @param j The JSON value to parse, expected to be an array of at least 4 elements representing the R, G, B, and A components of the color.
     * @param fallback A Color value to return if the JSON is not a valid array for parsing.
     * @return A Color constructed from the JSON array, or the fallback value if parsing fails.
     */
    Color ParseColor(const json& j, const Color& fallback) {
        if (!j.is_array() || j.size() < 4) {
            return fallback;
        }

        return Color{
            j.at(0).get<float>(),
            j.at(1).get<float>(),
            j.at(2).get<float>(),
            j.at(3).get<float>()
        };
    }

    /**
     * @brief Normalizes an animation asset file path for loading by converting it to an absolute path based on 
     * the project root, unless it is already absolute or marked as transient.
     * 
     * @param path The input file path to normalize.
     * @return A normalized file path that is absolute and can be used for loading assets, or the original path 
     * if it is empty, already absolute, or marked as transient.
     */
    std::string NormalizeAnimPathForLoad(const std::string& path) {
        if (path.empty() || !Engine::ProjectPaths::IsInitialized()) {
            return path;
        }
        if (path.rfind(kTransientPrefix, 0) == 0) {
            return path;
        }
        std::filesystem::path fsPath(path);
        if (fsPath.is_absolute()) {
            return path;
        }
        return Engine::ProjectPaths::ToAbsolutePath(path);
    }
}

namespace Animation {
    /**
     * @brief Retrieves the singleton instance of the AnimationAssetManager, which is responsible for managing animation clip and controller assets.
     * @return A reference to the AnimationAssetManager instance.
     */
    AnimationAssetManager& AnimationAssetManager::Get() {
        static AnimationAssetManager s_instance;
        return s_instance;
    }

    /**
     * @brief Computes a unique hash ID for an animation asset based on its file path, using the FNV-1a hashing algorithm.
     * @param path The file path of the animation asset to hash.
     * @return A 32-bit unsigned integer representing the hash of the file path, which can be used as a unique identifier for the asset.
     */
    uint32_t AnimationAssetManager::HashPath(const std::string& path) {
        return HashString(path);
    }

    /**
     * @brief Retrieves the ID of an existing animation clip asset by its file path, or loads it from disk if it doesn't exist in the manager.
     * The file path is normalized to an absolute path based on the project root, unless it is already absolute or marked as transient. 
     * If the asset is successfully loaded, its data is stored in the manager and a unique ID is returned.
     * 
     * @param path The file path of the animation clip asset to retrieve or load.
     * @return The unique ID of the animation clip asset, or 0 if the asset could not be loaded.
     */
    uint32_t AnimationAssetManager::GetOrLoadClip(const std::string& path) {
        if (path.empty()) {
            return 0;
        }

        // Normalize the path for loading, which converts it to an absolute path based on the project root if it's not already absolute or transient
        const std::string resolvedPath = NormalizeAnimPathForLoad(path);
        const auto it = m_clipPathToId.find(resolvedPath);
        
        // If the clip already exists in the manager, return its ID
        if (it != m_clipPathToId.end()) {
            return it->second;
        }

        // Load the clip from the JSON file at the resolved path
        AnimationClip2DData clip{};
        if (!LoadClipFromJson(resolvedPath, clip)) {
            return 0;
        }

        // Generate a unique ID for the clip based on the resolved path and store it in the manager
        const uint32_t id = HashPath(resolvedPath);
        m_clips[id] = std::move(clip);
        m_clipPathToId[resolvedPath] = id;

        return id;
    }

    /**
     * @brief Retrieves the ID of an existing animation controller asset by its file path, or loads it from disk if it doesn't exist in the manager.
     * The file path is normalized to an absolute path based on the project root, unless it is already absolute or marked as transient. 
     * If the asset is successfully loaded, its data is stored in the manager and a unique ID is returned.
     * 
     * @param path The file path of the animation controller asset to retrieve or load.
     * @return The unique ID of the animation controller asset, or 0 if the asset could not be loaded.
     */
    uint32_t AnimationAssetManager::GetOrLoadController(const std::string& path) {
        if (path.empty()) {
            return 0;
        }

        const std::string resolvedPath = NormalizeAnimPathForLoad(path);
        const auto it = m_controllerPathToId.find(resolvedPath);
        if (it != m_controllerPathToId.end()) {
            return it->second;
        }
        
        AnimationController2DData controller{};
        if (!LoadControllerFromJson(resolvedPath, controller)) {
            return 0;
        }
        
        const uint32_t id = HashPath(resolvedPath);
        m_controllers[id] = std::move(controller);
        
        m_controllerPathToId[resolvedPath] = id;
        return id;
    }

    /**
     * @brief Finds an animation clip asset by its unique ID, returning a pointer to the clip data if found or nullptr if not found.
     * @param id The unique ID of the animation clip asset to find.
     * @return A pointer to the AnimationClip2DData if found, or nullptr if not found in the manager.
     */
    const AnimationClip2DData* AnimationAssetManager::FindClip(uint32_t id) const {
        const auto it = m_clips.find(id);
        if (it == m_clips.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /**
     * @brief Finds an animation controller asset by its unique ID, returning a pointer to the controller data if found or nullptr if not found.
     * @param id The unique ID of the animation controller asset to find.
     * @return A pointer to the AnimationController2DData if found, or nullptr if not found in the manager.
     */
    const AnimationController2DData* AnimationAssetManager::FindController(uint32_t id) const {
        const auto it = m_controllers.find(id);
        if (it == m_controllers.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /**
     * @brief Creates a transient animation clip asset from the given clip data, generating a unique ID based on the clip's name and storing it in the manager.
     * @param clip The animation clip data to create a transient asset from.
     * @return The unique ID of the created transient animation clip asset.
     */
    uint32_t AnimationAssetManager::CreateTransientClip(const AnimationClip2DData& clip) {
        const uint32_t id = HashPath("transient_clip_" + clip.Name); // Use name-based id to keep stable per run.
        m_clips[id] = clip;
        return id;
    }

    /**
     * @brief Creates a transient animation controller asset from the given controller data, generating a unique ID based on the controller's entry state and storing it in the manager.
     * @param controller The animation controller data to create a transient asset from.
     * @return The unique ID of the created transient animation controller asset.
     */
    uint32_t AnimationAssetManager::CreateTransientController(const AnimationController2DData& controller) {
        const uint32_t id = HashPath("transient_controller_" + controller.EntryState);
        m_controllers[id] = controller;
        return id;
    }

    /**
     * @brief Creates a transient animation clip asset from the given clip data and associates it with the specified file path, allowing it to be retrieved by that path in the future.
     * The file path is normalized to an absolute path based on the project root, unless it is already absolute or marked as transient. 
     * The unique ID for the clip is generated based on the normalized path.
     * 
     * @param path The file path to associate with the transient animation clip asset.
     * @param clip The animation clip data to create a transient asset from.
     * @return The unique ID of the created transient animation clip asset.
     */
    uint32_t AnimationAssetManager::CreateTransientClipWithPath(const std::string& path, const AnimationClip2DData& clip) {
        const uint32_t id = HashPath(path);
        m_clips[id] = clip;
        m_clipPathToId[path] = id;
        return id;
    }

    /**
     * @brief Creates a transient animation controller asset from the given controller data and associates it with the specified file path, allowing it to be retrieved by that path in the future.
     * The file path is normalized to an absolute path based on the project root, unless it is already absolute or marked as transient. 
     * The unique ID for the controller is generated based on the normalized path.
     * 
     * @param path The file path to associate with the transient animation controller asset.
     * @param controller The animation controller data to create a transient asset from.
     * @return The unique ID of the created transient animation controller asset.
     */
    uint32_t AnimationAssetManager::CreateTransientControllerWithPath(const std::string& path, const AnimationController2DData& controller) {
        const uint32_t id = HashPath(path);
        m_controllers[id] = controller;
        m_controllerPathToId[path] = id;
        return id;
    }

    /**
     * @brief Loads an animation clip asset from a JSON file at the specified path, populating the provided AnimationClip2DData structure with the loaded data.
     * The file path is normalized to an absolute path based on the project root, unless it is already absolute or marked as transient. The JSON file is expected to have a specific structure that matches
     * the fields of AnimationClip2DData, including the sprite sheet information, frame durations, and per-frame data such as root motion, notifies, hitboxes, and attachments.
     * 
     * @param path The file path of the JSON file to load the animation clip from.
     * @param outClip A reference to an AnimationClip2DData structure that will be populated with the loaded data if the loading is successful.
     * @return true if the clip was successfully loaded from the JSON file, or false if there was an error during loading (e.g., file not found, JSON parsing error, missing required fields, etc.).
     */
    bool AnimationAssetManager::LoadClipFromJson(const std::string& path, AnimationClip2DData& outClip) {
        json j;
        if (!Serialization::Serializer::LoadJson(path, "anim", j)) {
            LOG_WARNING("[AnimationAssetManager] Failed to load clip: " << path);
            return false;
        }

        outClip.Name = j.value("name", std::string());

        // Load sprite sheet data from the "spriteSheet" object in the JSON, if it exists
        // This includes fields like texture paths, frame dimensions, sheet dimensions, frame count, playback speed, looping, etc.
        const auto& ss = j["spriteSheet"];
        if (ss.is_object()) {
            outClip.SpriteSheet.TexturePath = NormalizeAnimPathForLoad(ss.value("texture", std::string()));
            outClip.SpriteSheet.NormalTexturePath = NormalizeAnimPathForLoad(ss.value("normal", std::string()));
            outClip.SpriteSheet.FrameWidth = ss.value("frameWidth", 0);
            outClip.SpriteSheet.FrameHeight = ss.value("frameHeight", 0);
            outClip.SpriteSheet.SheetWidth = ss.value("sheetWidth", 0);
            outClip.SpriteSheet.SheetHeight = ss.value("sheetHeight", 0);
            outClip.SpriteSheet.StartFrame = ss.value("startFrame", 0);
            outClip.SpriteSheet.FrameCount = ss.value("frameCount", 0);
            outClip.SpriteSheet.Row = ss.value("row", 0);
            outClip.SpriteSheet.FrameOffset = ss.value("frameOffset", 0);
            outClip.SpriteSheet.FrameLength = ss.value("frameLength", 0);
            outClip.SpriteSheet.FramesPerSecond = ss.value("fps", 10.0f);
            outClip.SpriteSheet.Loop = ss.value("loop", true);
            outClip.SpriteSheet.UseRow = ss.value("useRow", false);
        }

        // Load frame durations from the "frameDurations" array in the JSON, if it exists
        if (j.contains("frameDurations") && j["frameDurations"].is_array()) {
            for (const auto& entry : j["frameDurations"]) {
                outClip.FrameDurations.push_back(entry.get<float>());
            }
        }

        // Load per-frame data from the "frames" array in the JSON, if it exists.
        // Each frame can have root motion, notifies, hitboxes, and attachments.
        if (j.contains("frames") && j["frames"].is_array()) {
            for (const auto& frameJson : j["frames"]) {
                AnimationFrameData2D frame{};
                if (frameJson.contains("rootMotion")) {
                    frame.RootMotion = ParseVector2D(frameJson["rootMotion"], frame.RootMotion);
                }
                if (frameJson.contains("notifies")) {
                    for (const auto& notify : frameJson["notifies"]) {
                        AnimationFrameNotify2D n{};
                        n.Name = notify.value("name", std::string());
                        frame.Notifies.push_back(std::move(n));
                    }
                }
                if (frameJson.contains("hitboxes")) {
                    for (const auto& hitbox : frameJson["hitboxes"]) {
                        AnimationFrameHitbox2D h{};
                        h.Name = hitbox.value("name", std::string());
                        if (hitbox.contains("offset")) {
                            h.Offset = ParseVector2D(hitbox["offset"], h.Offset);
                        }
                        if (hitbox.contains("size")) {
                            h.Size = ParseVector2D(hitbox["size"], h.Size);
                        }
                        if (hitbox.contains("color")) {
                            h.Color = ParseColor(hitbox["color"], h.Color);
                        }
                        frame.Hitboxes.push_back(std::move(h));
                    }
                }
                if (frameJson.contains("attachments")) {
                    for (const auto& attachment : frameJson["attachments"]) {
                        AnimationFrameAttachment2D a{};
                        a.Name = attachment.value("name", std::string());
                        if (attachment.contains("offset")) {
                            a.Offset = ParseVector2D(attachment["offset"], a.Offset);
                        }
                        frame.Attachments.push_back(std::move(a));
                    }
                }
                outClip.Frames.push_back(std::move(frame));
            }
        }

        return true;
    }

    /**
     * @brief Loads an animation controller asset from a JSON file at the specified path, populating the provided AnimationController2DData structure with the loaded data.
     * The file path is normalized to an absolute path based on the project root, unless it is already absolute or marked as transient. The JSON file is expected to have a specific structure that matches
     * the fields of AnimationController2DData, including the entry state, parameters, states, and transitions, with each parameter having a name, type, and default value, each state having
     * a name, clip path, speed, and loop flag, and each transition having from/to states, conditions, exit time, and duration.
     * 
     * @param path The file path of the JSON file to load the animation controller from.
     * @param outController A reference to an AnimationController2DData structure that will be populated with the loaded data if the loading is successful.
     * @return true if the controller was successfully loaded from the JSON file, or false if there was an error during loading (e.g., file not found, JSON parsing error, missing required fields, etc.).
     */
    bool AnimationAssetManager::LoadControllerFromJson(const std::string& path, AnimationController2DData& outController) {
        json j;
        if (!Serialization::Serializer::LoadJson(path, "animctrl", j)) {
            LOG_WARNING("[AnimationAssetManager] Failed to load controller: " << path);
            return false;
        }

        // Load entry state from the "entryState" field in the JSON, if it exists
        outController.EntryState = j.value("entryState", std::string());
        if (j.contains("parameters") && j["parameters"].is_array()) {
            for (const auto& param : j["parameters"]) {
                AnimationParamDef2D def{};
                def.Name = param.value("name", std::string());
                const std::string type = param.value("type", std::string("float"));
                if (type == "bool") {
                    def.Type = ParamType::Bool;
                    def.DefaultBool = param.value("default", false);
                } else if (type == "int") {
                    def.Type = ParamType::Int;
                    def.DefaultInt = param.value("default", 0);
                } else {
                    def.Type = ParamType::Float;
                    def.DefaultFloat = param.value("default", 0.0f);
                }
                outController.Parameters.push_back(std::move(def));
            }
        }

        // Load states from the "states" array in the JSON, if it exists. Each state has a name, clip path, speed, and loop flag.
        if (j.contains("states") && j["states"].is_array()) {
            for (const auto& state : j["states"]) {
                AnimationState2D s{};
                s.Name = state.value("name", std::string());
                s.ClipPath = NormalizeAnimPathForLoad(state.value("clip", std::string()));
                s.Speed = state.value("speed", 1.0f);
                s.Loop = state.value("loop", true);
                outController.States.push_back(std::move(s));
            }
        }

        // Load transitions from the "transitions" array in the JSON, if it exists. Each transition has from/to states, conditions, exit time, and duration.
        if (j.contains("transitions") && j["transitions"].is_array()) {
            for (const auto& trans : j["transitions"]) {
                AnimationTransition2D t{};
                t.FromState = trans.value("from", std::string());
                t.ToState = trans.value("to", std::string());
                t.ExitTime = trans.value("exitTime", 0.0f);
                t.Duration = trans.value("duration", 0.0f);
                if (trans.contains("conditions") && trans["conditions"].is_array()) {
                    for (const auto& cond : trans["conditions"]) {
                        AnimationTransitionCondition2D c{};
                        c.Param = cond.value("param", std::string());
                        const std::string op = cond.value("op", "==");
                        if (op == "!=") c.Op = CompareOp::NotEqual;
                        else if (op == "<") c.Op = CompareOp::Less;
                        else if (op == ">") c.Op = CompareOp::Greater;
                        else c.Op = CompareOp::Equal;
                        c.Value = cond.value("value", 0.0f);
                        t.Conditions.push_back(std::move(c));
                    }
                }
                outController.Transitions.push_back(std::move(t));
            }
        }

        return true;
    }
}
