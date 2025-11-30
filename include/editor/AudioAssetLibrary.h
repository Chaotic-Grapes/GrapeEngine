#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <algorithm>

class AudioAssetLibrary {
public:

	struct ClipInfo {
		uint32_t id; //cueID path is in uint32
		std::string name; //name of the clip
		std::string path; //clip's path
	};

	static AudioAssetLibrary& Get() {
		// reference function to always refer to this one static
		// local instance that is reusable and only one
		static AudioAssetLibrary s_instance;
		return s_instance;
	}

    /*
      Refresh(audioRoot)
   
      Scans a root folder (e.g. "assets/Audio") and ALL subfolders.

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

      This should be called once when the editor loads
  */
    void Refresh(const std::string& audioRoot) {
        m_clips.clear();
        m_byId.clear();
        m_byPath.clear();

        namespace fs = std::filesystem;

        fs::path root(audioRoot);

        if (!fs::exists(root) || !fs::is_directory(root))
            return;

        // Recursively walk all subfolders under assets/Audio/
        for (auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file())
                continue;

            // Filter audio formats the engine supports
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".wav" && ext != ".ogg" && ext != ".mp3" && ext != ".flac")
                continue;

            // Build normalized path "assets/Audio/.../file.wav"
            std::string relPath = Normalize(entry.path().string());

            // Name = filename without extension
            std::string name = entry.path().stem().string();

            // Unique stable ID based on path hash
            uint32_t id = static_cast<uint32_t>(std::hash<std::string>{}(relPath));

            // Construct clip info
            ClipInfo info{ id, name, relPath };

            // Store three different ways for fast lookup
            m_clips.push_back(info);
            m_byId[id] = info;
            m_byPath[relPath] = info;
        }
    }


    // Returns ALL loaded audio clips for UI dropdowns or listing
    const std::vector<ClipInfo>& GetAllClips() const { return m_clips; }


    // Lookup a clip by its CueId (used by AudioSource)
    const ClipInfo* FindById(uint32_t id) const {
        auto it = m_byId.find(id);
        return (it != m_byId.end()) ? &it->second : nullptr;
    }


    // Lookup by path (used for drag & drop)
    const ClipInfo* FindByPath(const std::string& p) const {
        std::string norm = Normalize(p);
        auto it = m_byPath.find(norm);
        return (it != m_byPath.end()) ? &it->second : nullptr;
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
        std::string norm = Normalize(p);
        auto it = m_byPath.find(norm);
        if (it != m_byPath.end())
            return it->second;

        // Build metadata for new file
        std::string name = std::filesystem::path(norm).stem().string();
        uint32_t id = static_cast<uint32_t>(std::hash<std::string>{}(norm));

        ClipInfo info{ id, name, norm };

        m_clips.push_back(info);
        m_byId[id] = info;
        m_byPath[norm] = info;

        return m_clips.back();
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
            "assets\Audio\BGMs\file.wav"
        becomes:
            "assets/Audio/BGMs/file.wav"

        This is critical for:
            - consistent hashing
            - consistent lookup
            - matching drag-drop paths
    */
    static std::string Normalize(std::string p) {
        std::replace(p.begin(), p.end(), '\\', '/');
        return p;
    }

    // List of all audio clips (for UI dropdowns)
    std::vector<ClipInfo> m_clips;

    // Fast lookup: id -> ClipInfo
    std::unordered_map<uint32_t, ClipInfo> m_byId;

    // Fast lookup: path -> ClipInfo
    std::unordered_map<std::string, ClipInfo> m_byPath;
};



