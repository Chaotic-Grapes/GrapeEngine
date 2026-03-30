/* Start Header *****************************************************************/
/*!
\file    SaveGameManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Save game service with profile-based save slots, metadata indexing, and 
deterministic scene snapshot capture/loading.
*/
/* End Header *******************************************************************/

#ifndef SAVEGAMEMANAGER_H
#define SAVEGAMEMANAGER_H

#include "Export.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Scenes {
    class SceneManager;
}

namespace Services {

    /**
     * @brief Public metadata for one save slot.
     */
    struct GRAPEENGINE_API SaveSlotMetadata {
        uint32_t Slot = 0;
        std::string DisplayName;
        std::string SceneName;
        std::string SourceScenePath;
        std::string SnapshotFile;
        std::string SavedAtUtc;
        std::string SaveVersion;
        std::string UserDataJson;
        double PlayTimeSeconds = 0.0;
        uint64_t SceneFileBytes = 0;
    };

    /**
     * @brief Save-game manager with profile-based slot isolation.
     */
    class GRAPEENGINE_API SaveGameManager {
    public:
        /**
         * @brief Set active save profile id.
         *
         * @param profileId Profile identifier (empty uses "default").
         * @return True when profile id was accepted.
         */
        bool SetActiveProfile(const std::string& profileId);

        /**
         * @brief Get currently active profile id.
         *
         * @return Active profile identifier.
         */
        const std::string& GetActiveProfile() const { return m_activeProfile; }

        /**
         * @brief Save active scene into a numbered slot.
         *
         * @param sceneManager Scene manager owning current runtime scene.
         * @param slot Save slot id.
         * @param displayName Optional user-friendly slot name.
         * @param userDataJson Optional custom JSON payload for gameplay state.
         * @return True on success.
         */
        bool SaveSlot(Scenes::SceneManager& sceneManager, uint32_t slot,
            const std::string& displayName = std::string(),
            const std::string& userDataJson = std::string());

        /**
         * @brief Load a slot into the active scene (or first available scene slot).
         *
         * @param sceneManager Scene manager receiving loaded data.
         * @param slot Save slot id.
         * @return True on success.
         */
        bool LoadSlot(Scenes::SceneManager& sceneManager, uint32_t slot);

        /**
         * @brief Delete one save slot.
         *
         * @param slot Save slot id.
         * @return True when removed.
         */
        bool DeleteSlot(uint32_t slot);

        /**
         * @brief Check whether a slot currently exists.
         *
         * @param slot Save slot id.
         * @return True if slot has a valid manifest and snapshot.
         */
        bool HasSlot(uint32_t slot) const;

        /**
         * @brief List all slots from active profile.
         *
         * @return Sorted metadata entries by slot id.
         */
        std::vector<SaveSlotMetadata> ListSlots() const;

        /**
         * @brief Query one slot metadata.
         *
         * @param slot Save slot id.
         * @return Slot metadata if found.
         */
        std::optional<SaveSlotMetadata> GetSlotMetadata(uint32_t slot) const;

        /**
         * @brief Export slot metadata list as JSON.
         *
         * @return UTF-8 JSON array string.
         */
        std::string ExportSlotsJson() const;

        /**
         * @brief Export one slot metadata as JSON.
         *
         * @param slot Save slot id.
         * @return UTF-8 JSON object string, or empty object when missing.
         */
        std::string ExportSlotJson(uint32_t slot) const;

    private:
        std::string m_activeProfile = "default";
    };

}

#endif
