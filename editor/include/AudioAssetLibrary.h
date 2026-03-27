/**
 * @Name: Dalton Koh, 2403250
 * @email: d.koh@digipen.edu
 * @file AudioAssetLibrary.h
 * @brief Singleton asset indexing system for audio file discovery and CueId management.
 *
 * @details
 * Provides a centralized audio asset management system that recursively scans the
 * project root to discover all supported audio files (.wav, .ogg, .mp3, .flac),
 * automatically generates stable hash-based CueIds from normalized file paths, and
 * maintains efficient lookup maps for path-to-CueId and CueId-to-path resolution.
 */

#pragma once
#include <string>
#include <vector>
#include "audio/AudioCueRegistry.h"

class AudioAssetLibrary {
public:

	using ClipInfo = Audio::AudioCueRegistry::CueInfo;

    /**
     * @brief Access the singleton AudioAssetLibrary instance.
     * @return Reference to the global library.
     */
	static AudioAssetLibrary& Get() {
		// reference function to always refer to this one static
		// local instance that is reusable and only "ONE"
		static AudioAssetLibrary s_instance;
		return s_instance;
	}

    /**
     * @brief Scan and refresh the audio asset registry from disk.
     * @param audioRoot Root directory to recursively scan for supported audio files.
     */
    void Refresh(const std::string& audioRoot) {
        m_registry.Refresh(audioRoot);
    }


    /**
     * @brief Get all currently registered audio clips.
     * @return Const vector of clip metadata.
     */
    const std::vector<ClipInfo>& GetAllClips() const { return m_registry.GetAll(); }


    /**
     * @brief Find a clip by CueId.
     * @param id Cue identifier.
     * @return Pointer to clip metadata, or nullptr if not found.
     */
    const ClipInfo* FindById(uint32_t id) const {
        return m_registry.FindById(id);
    }


    /**
     * @brief Find a clip by normalized asset path.
     * @param p Clip path.
     * @return Pointer to clip metadata, or nullptr if not found.
     */
    const ClipInfo* FindByPath(const std::string& p) const {
        return m_registry.FindByPath(p);
    }


    /**
     * @brief Register a clip path discovered at runtime.
     * @param p Clip path to register.
     * @return Reference to the registered or existing clip metadata.
     */
    const ClipInfo& Register(const std::string& p) {
        return m_registry.Register(p);
    }


private:

    /**
     * @brief Construct singleton instance.
     */
    AudioAssetLibrary() = default;


    /*
        Normalize(path)
        ----------------
        Ensures all paths look consistent internally:
            "Project\\Assets\\Audio\\BGMs\\file.wav"
        becomes:
            "Project/Assets/Audio/BGMs/file.wav"

        This is critical for:
            - consistent hashing
            - consistent lookup
            - matching drag-drop paths
    */
    Audio::AudioCueRegistry m_registry;
};



