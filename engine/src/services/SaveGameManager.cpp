/* Start Header *****************************************************************/
/*!
\file    SaveGameManager.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implements profile-based save slots with atomic manifest/index writes and
scene snapshot persistence through SceneManager serialization.

Complexity notes:
- SaveSlot: O(E log E + M), where E is entity count and M is metadata write cost.
- LoadSlot: O(L), where L is scene deserialization workload.
- ListSlots/GetSlot: O(S), where S is slot count in profile index.

Cache notes:
- Cold path filesystem and JSON parsing dominate cost.
- Runtime hot loops are unaffected because operations are explicit IO calls.
*/
/* End Header *******************************************************************/

#include "services/SaveGameManager.h"

#include "core/Logger.h"
#include "core/ProjectPaths.h"
#include "scene/Scene.h"
#include "scene/SceneManager.h"
#include "services/TimeSystem.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

using json = nlohmann::json;

namespace Services {

    namespace {
        constexpr int kSaveSchemaVersion = 1;

        /**
         * @brief Function purpose: sanitize profile name for filesystem-safe usage.
         * Input parameters: rawProfileId User-provided profile identifier.
         * Output/return values: sanitized profile id, or "default" when empty.
         */
        std::string SanitizeProfileId(const std::string& rawProfileId) {
            if (rawProfileId.empty()) {
                return "default";
            }

            std::string out;
            out.reserve(rawProfileId.size());
            for (unsigned char c : rawProfileId) {
                if ((c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '_' || c == '-' || c == '.') {
                    out.push_back(static_cast<char>(c));
                }
            }

            return out.empty() ? "default" : out;
        }

        /**
         * @brief Function purpose: build profile root path under project saves.
         * Input parameters: profileId Active sanitized profile id.
         * Output/return values: absolute profile folder path.
         */
        std::filesystem::path ProfileRootPath(const std::string& profileId) {
            return std::filesystem::path(Engine::ProjectPaths::GetSavesPath()) / "profiles" / profileId;
        }

        /**
         * @brief Function purpose: build deterministic slot folder path.
         * Input parameters: profileId Active profile id, slot Slot index.
         * Output/return values: absolute slot folder path.
         */
        std::filesystem::path SlotRootPath(const std::string& profileId, uint32_t slot) {
            std::ostringstream oss;
            oss << "slot_" << std::setw(3) << std::setfill('0') << slot;
            return ProfileRootPath(profileId) / oss.str();
        }

        /**
         * @brief Function purpose: return index.json path for active profile.
         * Input parameters: profileId Active profile id.
         * Output/return values: absolute index file path.
         */
        std::filesystem::path IndexPath(const std::string& profileId) {
            return ProfileRootPath(profileId) / "index.json";
        }

        /**
         * @brief Function purpose: return manifest path for one slot.
         * Input parameters: profileId Active profile id, slot Slot index.
         * Output/return values: absolute manifest file path.
         */
        std::filesystem::path ManifestPath(const std::string& profileId, uint32_t slot) {
            return SlotRootPath(profileId, slot) / "manifest.json";
        }

        /**
         * @brief Function purpose: return snapshot scene path for one slot.
         * Input parameters: profileId Active profile id, slot Slot index.
         * Output/return values: absolute scene snapshot path.
         */
        std::filesystem::path SnapshotScenePath(const std::string& profileId, uint32_t slot) {
            return SlotRootPath(profileId, slot) / "snapshot.scene";
        }

        /**
         * @brief Function purpose: generate UTC timestamp string in ISO-8601 format.
         * Input parameters: none.
         * Output/return values: UTC timestamp string.
         */
        std::string UtcNowIso8601() {
            using Clock = std::chrono::system_clock;
            const auto now = Clock::now();
            const std::time_t nowTime = Clock::to_time_t(now);

            std::tm utc{};
#ifdef _WIN32
            gmtime_s(&utc, &nowTime);
#else
            gmtime_r(&nowTime, &utc);
#endif

            std::ostringstream oss;
            oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
            return oss.str();
        }

        /**
         * @brief Function purpose: write JSON file atomically via temp file swap.
         * Input parameters: path Destination path, j JSON payload.
         * Output/return values: true on successful atomic replacement.
         */
        bool WriteJsonAtomic(const std::filesystem::path& path, const json& j) {
            try {
                const auto parent = path.parent_path();
                if (!parent.empty()) {
                    std::filesystem::create_directories(parent);
                }

                const std::filesystem::path tempPath = path.string() + ".tmp";
                {
                    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
                    if (!out.is_open()) {
                        LOG_ERROR("[SaveGame] Failed to open temp file for write: " << tempPath.string());
                        return false;
                    }
                    out << j.dump(4);
                    out.flush();
                    if (!out.good()) {
                        LOG_ERROR("[SaveGame] Failed writing temp file: " << tempPath.string());
                        return false;
                    }
                }

                std::error_code ec;
                std::filesystem::remove(path, ec);
                ec.clear();
                std::filesystem::rename(tempPath, path, ec);
                if (ec) {
                    LOG_ERROR("[SaveGame] Atomic rename failed for " << path.string() << ": " << ec.message());
                    std::filesystem::remove(tempPath, ec);
                    return false;
                }

                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("[SaveGame] WriteJsonAtomic exception: " << e.what());
                return false;
            }
        }

        /**
         * @brief Function purpose: parse JSON file if it exists.
         * Input parameters: path File path, outJson Destination JSON object.
         * Output/return values: true when file exists and parses correctly.
         */
        bool ReadJsonFile(const std::filesystem::path& path, json& outJson) {
            std::ifstream in(path, std::ios::binary);
            if (!in.is_open()) {
                return false;
            }

            try {
                in >> outJson;
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("[SaveGame] JSON parse failed for " << path.string() << ": " << e.what());
                return false;
            }
        }

        /**
         * @brief Function purpose: create default profile index payload.
         * Input parameters: profileId Profile identifier.
         * Output/return values: initialized index JSON.
         */
        json MakeDefaultIndex(const std::string& profileId) {
            json index;
            index["SchemaVersion"] = kSaveSchemaVersion;
            index["ProfileId"] = profileId;
            index["Slots"] = json::array();
            return index;
        }

        /**
         * @brief Function purpose: convert slot metadata to JSON object.
         * Input parameters: meta Slot metadata.
         * Output/return values: JSON object representation.
         */
        json ToJson(const SaveSlotMetadata& meta) {
            json j;
            j["Slot"] = meta.Slot;
            j["DisplayName"] = meta.DisplayName;
            j["SceneName"] = meta.SceneName;
            j["SourceScenePath"] = meta.SourceScenePath;
            j["SnapshotFile"] = meta.SnapshotFile;
            j["SavedAtUtc"] = meta.SavedAtUtc;
            j["SaveVersion"] = meta.SaveVersion;
            j["PlayTimeSeconds"] = meta.PlayTimeSeconds;
            j["SceneFileBytes"] = meta.SceneFileBytes;

            if (!meta.UserDataJson.empty()) {
                try {
                    j["UserData"] = json::parse(meta.UserDataJson);
                }
                catch (...) {
                    j["UserDataRaw"] = meta.UserDataJson;
                }
            }

            return j;
        }

        /**
         * @brief Function purpose: convert JSON object to slot metadata.
         * Input parameters: j Source JSON object.
         * Output/return values: parsed SaveSlotMetadata value.
         */
        SaveSlotMetadata FromJson(const json& j) {
            SaveSlotMetadata meta;
            meta.Slot = j.value("Slot", 0u);
            meta.DisplayName = j.value("DisplayName", std::string());
            meta.SceneName = j.value("SceneName", std::string());
            meta.SourceScenePath = j.value("SourceScenePath", std::string());
            meta.SnapshotFile = j.value("SnapshotFile", std::string());
            meta.SavedAtUtc = j.value("SavedAtUtc", std::string());
            meta.SaveVersion = j.value("SaveVersion", std::string("1.0"));
            meta.PlayTimeSeconds = j.value("PlayTimeSeconds", 0.0);
            meta.SceneFileBytes = j.value("SceneFileBytes", static_cast<uint64_t>(0));

            if (j.contains("UserData")) {
                meta.UserDataJson = j["UserData"].dump();
            }
            else {
                meta.UserDataJson = j.value("UserDataRaw", std::string());
            }

            return meta;
        }

        /**
         * @brief Function purpose: ensure profile folder structure exists.
         * Input parameters: profileId Active profile identifier.
         * Output/return values: true when profile directories are ready.
         */
        bool EnsureProfileLayout(const std::string& profileId) {
            try {
                std::filesystem::create_directories(ProfileRootPath(profileId));
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("[SaveGame] Failed creating profile layout: " << e.what());
                return false;
            }
        }

        /**
         * @brief Function purpose: load index.json, rebuilding from manifests on demand.
         * Input parameters: profileId Active profile identifier, outIndex Destination payload.
         * Output/return values: true when index is loaded or rebuilt.
         */
        bool LoadOrRebuildIndex(const std::string& profileId, json& outIndex) {
            const auto indexPath = IndexPath(profileId);
            if (ReadJsonFile(indexPath, outIndex)) {
                if (!outIndex.contains("Slots") || !outIndex["Slots"].is_array()) {
                    outIndex = MakeDefaultIndex(profileId);
                }
                return true;
            }

            outIndex = MakeDefaultIndex(profileId);
            const auto profileRoot = ProfileRootPath(profileId);
            if (!std::filesystem::exists(profileRoot)) {
                return true;
            }

            json slots = json::array();
            for (const auto& entry : std::filesystem::directory_iterator(profileRoot)) {
                if (!entry.is_directory()) {
                    continue;
                }

                const auto manifest = entry.path() / "manifest.json";
                json manifestJson;
                if (!ReadJsonFile(manifest, manifestJson)) {
                    continue;
                }

                slots.push_back(ToJson(FromJson(manifestJson)));
            }

            std::sort(slots.begin(), slots.end(), [](const json& a, const json& b) {
                return a.value("Slot", 0u) < b.value("Slot", 0u);
            });

            outIndex["Slots"] = std::move(slots);
            WriteJsonAtomic(indexPath, outIndex);
            return true;
        }

        /**
         * @brief Function purpose: upsert slot metadata into profile index.
         * Input parameters: profileId Active profile id, meta New metadata.
         * Output/return values: true when index was persisted.
         */
        bool UpsertIndexSlot(const std::string& profileId, const SaveSlotMetadata& meta) {
            json index;
            if (!LoadOrRebuildIndex(profileId, index)) {
                return false;
            }

            auto& slots = index["Slots"];
            bool updated = false;
            for (auto& slot : slots) {
                if (slot.value("Slot", 0u) == meta.Slot) {
                    slot = ToJson(meta);
                    updated = true;
                    break;
                }
            }

            if (!updated) {
                slots.push_back(ToJson(meta));
            }

            std::sort(slots.begin(), slots.end(), [](const json& a, const json& b) {
                return a.value("Slot", 0u) < b.value("Slot", 0u);
            });

            return WriteJsonAtomic(IndexPath(profileId), index);
        }

        /**
         * @brief Function purpose: remove slot metadata from index file.
         * Input parameters: profileId Active profile id, slot Slot index.
         * Output/return values: true when index was persisted.
         */
        bool RemoveIndexSlot(const std::string& profileId, uint32_t slot) {
            json index;
            if (!LoadOrRebuildIndex(profileId, index)) {
                return false;
            }

            json filtered = json::array();
            for (const auto& entry : index["Slots"]) {
                if (entry.value("Slot", 0u) != slot) {
                    filtered.push_back(entry);
                }
            }
            index["Slots"] = std::move(filtered);

            return WriteJsonAtomic(IndexPath(profileId), index);
        }

        /**
         * @brief Function purpose: compute deterministic entity order for serialization.
         * Input parameters: world Source ECS world.
         * Output/return values: sorted entity index vector.
         */
        std::vector<uint32_t> BuildDeterministicEntityOrder(const ECS::World& world) {
            std::vector<uint32_t> order;
            order.reserve(256);
            world.Each([&](const ECS::Entity e) {
                order.push_back(e.Index);
            });
            std::sort(order.begin(), order.end());
            return order;
        }
    }

    bool SaveGameManager::SetActiveProfile(const std::string& profileId) {
        const std::string sanitized = SanitizeProfileId(profileId);
        if (!EnsureProfileLayout(sanitized)) {
            return false;
        }
        m_activeProfile = sanitized;
        return true;
    }

    bool SaveGameManager::SaveSlot(Scenes::SceneManager& sceneManager, uint32_t slot,
        const std::string& displayName, const std::string& userDataJson) {
        if (!EnsureProfileLayout(m_activeProfile)) {
            return false;
        }

        Scenes::Scene* activeScene = sceneManager.GetActive();
        const size_t activeIndex = sceneManager.GetActiveIndex();
        if (!activeScene || activeIndex == static_cast<size_t>(-1)) {
            LOG_ERROR("[SaveGame] SaveSlot failed: no active scene");
            return false;
        }

        const auto slotRoot = SlotRootPath(m_activeProfile, slot);
        const auto manifestPath = ManifestPath(m_activeProfile, slot);
        const auto snapshotPath = SnapshotScenePath(m_activeProfile, slot);

        try {
            std::filesystem::create_directories(slotRoot);
        }
        catch (const std::exception& e) {
            LOG_ERROR("[SaveGame] Failed creating slot folder: " << e.what());
            return false;
        }

        const std::vector<uint32_t> entityOrder = BuildDeterministicEntityOrder(activeScene->GetWorld());
        const std::string saveName = displayName.empty() ? activeScene->GetName() : displayName;

        if (!sceneManager.SaveScene(activeIndex, snapshotPath.string(), saveName, "1.0", &entityOrder)) {
            LOG_ERROR("[SaveGame] SaveSlot failed while serializing scene");
            return false;
        }

        SaveSlotMetadata metadata;
        metadata.Slot = slot;
        metadata.DisplayName = saveName;
        metadata.SceneName = activeScene->GetName();
        metadata.SourceScenePath = activeScene->GetPath();
        metadata.SnapshotFile = snapshotPath.filename().string();
        metadata.SavedAtUtc = UtcNowIso8601();
        metadata.SaveVersion = "1.0";
        metadata.UserDataJson = userDataJson;
        metadata.PlayTimeSeconds = TimeSystem::Instance().GetRealTimeSinceStart();

        std::error_code ec;
        metadata.SceneFileBytes = std::filesystem::file_size(snapshotPath, ec);
        if (ec) {
            metadata.SceneFileBytes = 0;
        }

        const json manifestJson = ToJson(metadata);
        if (!WriteJsonAtomic(manifestPath, manifestJson)) {
            LOG_ERROR("[SaveGame] Failed writing slot manifest");
            return false;
        }

        if (!UpsertIndexSlot(m_activeProfile, metadata)) {
            LOG_ERROR("[SaveGame] Failed updating slot index");
            return false;
        }

        LOG_INFO("[SaveGame] Saved slot " << slot << " for profile '" << m_activeProfile << "'");
        return true;
    }

    bool SaveGameManager::LoadSlot(Scenes::SceneManager& sceneManager, uint32_t slot) {
        const auto slotMeta = GetSlotMetadata(slot);
        if (!slotMeta.has_value()) {
            LOG_ERROR("[SaveGame] LoadSlot failed: slot " << slot << " not found");
            return false;
        }

        const auto slotRoot = SlotRootPath(m_activeProfile, slot);
        std::filesystem::path snapshotPath = slotRoot / slotMeta->SnapshotFile;
        if (slotMeta->SnapshotFile.empty()) {
            snapshotPath = SnapshotScenePath(m_activeProfile, slot);
        }

        if (!std::filesystem::exists(snapshotPath)) {
            LOG_ERROR("[SaveGame] LoadSlot failed: snapshot missing at " << snapshotPath.string());
            return false;
        }

        size_t targetIndex = sceneManager.GetActiveIndex();
        if (targetIndex == static_cast<size_t>(-1)) {
            if (sceneManager.GetSceneCount() == 0) {
                targetIndex = sceneManager.AddScene(new Scenes::Scene());
            }
            else {
                targetIndex = 0;
            }
        }

        if (!sceneManager.LoadScene(targetIndex, snapshotPath.string())) {
            LOG_ERROR("[SaveGame] LoadSlot failed while deserializing scene");
            return false;
        }

        sceneManager.SetActiveImmediate(targetIndex);
        LOG_INFO("[SaveGame] Loaded slot " << slot << " for profile '" << m_activeProfile << "'");
        return true;
    }

    bool SaveGameManager::DeleteSlot(uint32_t slot) {
        const auto slotRoot = SlotRootPath(m_activeProfile, slot);
        std::error_code ec;
        const uintmax_t removed = std::filesystem::remove_all(slotRoot, ec);
        if (ec || removed == 0) {
            LOG_WARNING("[SaveGame] DeleteSlot: slot " << slot << " did not exist or failed to remove");
            return false;
        }

        if (!RemoveIndexSlot(m_activeProfile, slot)) {
            LOG_WARNING("[SaveGame] Slot removed but index update failed for slot " << slot);
        }

        return true;
    }

    bool SaveGameManager::HasSlot(uint32_t slot) const {
        const auto manifest = ManifestPath(m_activeProfile, slot);
        json j;
        if (!ReadJsonFile(manifest, j)) {
            return false;
        }

        const SaveSlotMetadata meta = FromJson(j);
        const std::filesystem::path snapshot = SlotRootPath(m_activeProfile, slot) /
            (meta.SnapshotFile.empty() ? "snapshot.scene" : meta.SnapshotFile);
        return std::filesystem::exists(snapshot);
    }

    std::vector<SaveSlotMetadata> SaveGameManager::ListSlots() const {
        std::vector<SaveSlotMetadata> out;
        json index;
        if (!LoadOrRebuildIndex(m_activeProfile, index)) {
            return out;
        }

        const auto& slots = index["Slots"];
        out.reserve(slots.size());
        for (const auto& entry : slots) {
            out.push_back(FromJson(entry));
        }

        std::sort(out.begin(), out.end(), [](const SaveSlotMetadata& a, const SaveSlotMetadata& b) {
            return a.Slot < b.Slot;
        });
        return out;
    }

    std::optional<SaveSlotMetadata> SaveGameManager::GetSlotMetadata(uint32_t slot) const {
        json index;
        if (LoadOrRebuildIndex(m_activeProfile, index)) {
            for (const auto& entry : index["Slots"]) {
                if (entry.value("Slot", 0u) == slot) {
                    return FromJson(entry);
                }
            }
        }

        json manifest;
        if (ReadJsonFile(ManifestPath(m_activeProfile, slot), manifest)) {
            return FromJson(manifest);
        }

        return std::nullopt;
    }

    std::string SaveGameManager::ExportSlotsJson() const {
        json arr = json::array();
        for (const auto& slot : ListSlots()) {
            arr.push_back(ToJson(slot));
        }
        return arr.dump();
    }

    std::string SaveGameManager::ExportSlotJson(uint32_t slot) const {
        const auto meta = GetSlotMetadata(slot);
        if (!meta.has_value()) {
            return json::object().dump();
        }
        return ToJson(meta.value()).dump();
    }

}
