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

	static AudioAssetLibrary& Get() {
		// reference function to always refer to this one static
		// local instance that is reusable and only "ONE"
		static AudioAssetLibrary s_instance;
		return s_instance;
	}

    /*
      Refresh(audioRoot)
      -------------------
      Scans a root folder (e.g. project root) and ALL subfolders.

      Finds all supported audio file formats:
          .wav, .ogg, .mp3, .flac

      For each audio file:
          - Normalize its path (so \ becomes /)
          - Extract its filename (without extension)
          - Generate a stable ID (hash of path)
          - Store ClipInfo in:
              m_clips          (vector of all clips)
              m_byId           (lookup ID -> info)
              m_byPath         (lookup path -> info)

      This should be called ONCE when the editor loads,
      or anytime the asset directory changes (optional).
  */
    void Refresh(const std::string& audioRoot) {
        m_registry.Refresh(audioRoot);
    }


    // Returns ALL loaded audio clips for UI dropdowns or listing
    const std::vector<ClipInfo>& GetAllClips() const { return m_registry.GetAll(); }


    // Lookup a clip by its CueId (used by AudioSource)
    const ClipInfo* FindById(uint32_t id) const {
        return m_registry.FindById(id);
    }


    // Lookup by path (used for drag & drop)
    const ClipInfo* FindByPath(const std::string& p) const {
        return m_registry.FindByPath(p);
    }


    /*
        Register(p)
        -----------

        Used when an audio file is DISCOVERED dynamically,
        such as when you drag a ".wav" from AssetBrowser onto the AudioSource inspector.

        If the file is NOT part of the current scan:
            - Normalize path
            - Generate a new ID
            - Insert into clip list
            - Return that clip info

        Allows the editor to support drag-and-drop addition of audio without rescanning everything.
    */
    const ClipInfo& Register(const std::string& p) {
        return m_registry.Register(p);
    }


private:

    /*
        Private constructor:
        --------------------
        Prevents creating new instances elsewhere.
        Only Get() can produce the one global instance.
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



