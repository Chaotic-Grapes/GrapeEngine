/* Start Header *****************************************************************/
/*!
\file   ResourceManager.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th January 2026
\brief
Implements the ResourceManager class for centralized asset management.

Features:
- Template specializations for Texture, AudioData, Font, Shader and PrefabData loading
- Automatic caching system to prevent redundant file operations
- File validation and error handling with logging
- Support for multiple audio formats (WAV, MP3, OGG, FLAC, M4A, AAC)
- Shader loading with automatic .vert/.frag extension handling
- Prefab loading with JSON content storage
- Cache management utilities for memory optimization
*/
/* End Header *******************************************************************/

#include "services/ResourceManager.h"
#include "core/Logger.h"
#include "core/messaging/MessageSystem.h"
#include <fstream>
#include <algorithm>

// Cache map accessors (template specializations)
// When T = Texture, return texture cache
template<>
std::unordered_map<std::string, std::shared_ptr<Texture>>&
ResourceManager::GetCacheMap<Texture>() { return m_textures; }

// When T = AudioData, return audio cache
template<>
std::unordered_map<std::string, std::shared_ptr<AudioData>>&
ResourceManager::GetCacheMap<AudioData>() { return m_audioFiles; }

// When T = Font, return font cache
template<>
std::unordered_map<std::string, std::shared_ptr<Font>>&
ResourceManager::GetCacheMap<Font>() { return m_fonts; }

// When T = Shader, return shader cache
template<>
std::unordered_map<std::string, std::shared_ptr<Shader>>&
ResourceManager::GetCacheMap<Shader>() { return m_shaders; }

// When T = PrefabData, return prefab cache
template<>
std::unordered_map<std::string, std::shared_ptr<PrefabData>>&
ResourceManager::GetCacheMap<PrefabData>() { return m_prefabs; }

// When T = RawData, return raw data cache
template<>
std::unordered_map<std::string, std::shared_ptr<RawData>>&
ResourceManager::GetCacheMap<RawData>() { return m_rawData; }
// When T = Texture, return texture owner map
template<>
std::unordered_map<std::string, std::unordered_set<std::string>>&
ResourceManager::GetOwnerMap<Texture>() { return m_textureOwners; }

// When T = AudioData, return audio owner map
template<>
std::unordered_map<std::string, std::unordered_set<std::string>>&
ResourceManager::GetOwnerMap<AudioData>() { return m_audioOwners; }

// When T = Font, return font owner map
template<>
std::unordered_map<std::string, std::unordered_set<std::string>>&
ResourceManager::GetOwnerMap<Font>() { return m_fontOwners; }

// When T = Shader, return shader owner map
template<>
std::unordered_map<std::string, std::unordered_set<std::string>>&
ResourceManager::GetOwnerMap<Shader>() { return m_shaderOwners; }

// When T = PrefabData, return prefab owner map
template<>
std::unordered_map<std::string, std::unordered_set<std::string>>&
ResourceManager::GetOwnerMap<PrefabData>() { return m_prefabOwners; }

// When T = RawData, return raw data owner map
template<>
std::unordered_map<std::string, std::unordered_set<std::string>>&
ResourceManager::GetOwnerMap<RawData>() { return m_rawDataOwners; }

// Generic Get function: handles caching logic for all asset types
template<typename T>std::shared_ptr<T> ResourceManager::Get(const std::string& name) {
    // Get reference to appropriate cache (m_textures or m_audioFiles)
    auto& cache = GetCacheMap<T>();

    // Check cache first
    // typeid(T).name() returns the type name as a string (e.g. "Texture", "AudioData")
    auto it = cache.find(name);
    if (it != cache.end()) {
		// Only log the first time we hit this asset in cache to avoid spamming logs for frequently used assets
        static std::unordered_set<std::string> loggedHits;
        if (loggedHits.insert(name).second) {
            LOG_DEBUG("[CACHE HIT] " << typeid(T).name() << ": " << name);
        }
        TrackOwner<T>(name);
        return it->second;  // Cache hit
    }

    // Not in cache: try to load
    LOG_DEBUG("[CACHE MISS] Loading " << typeid(T).name() << ": " << name);
    auto resource = Load<T>(name);

    if (resource) {
        cache[name] = resource;
        TrackOwner<T>(name);
        LOG_DEBUG("Cached " << typeid(T).name() << ": " << name);
    }
    else {
        LOG_ERROR("Failed to load " << typeid(T).name() << ": " << name);
    }

    return resource;
}

// Explicit instantiations with export declarations
// When other files call RM.Get<T>(), the linker can find them
template GRAPEENGINE_API std::shared_ptr<Texture> ResourceManager::Get<Texture>(const std::string&);
template GRAPEENGINE_API std::shared_ptr<AudioData> ResourceManager::Get<AudioData>(const std::string&);
template GRAPEENGINE_API std::shared_ptr<Font> ResourceManager::Get<Font>(const std::string&);
template GRAPEENGINE_API std::shared_ptr<Shader> ResourceManager::Get<Shader>(const std::string&);
template GRAPEENGINE_API std::shared_ptr<PrefabData> ResourceManager::Get<PrefabData>(const std::string&);
template GRAPEENGINE_API std::shared_ptr<RawData> ResourceManager::Get<RawData>(const std::string&);

// Loading function for textures
template<>std::shared_ptr<Texture> ResourceManager::Load<Texture>(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    // If not found, return nullptr immediately (fail fast)
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    try {
        // Use the existing Texture constructor to load the file
        auto texture = std::make_shared<Texture>(filePath);

        // Check if texture loaded successfully
        if (texture->ID() == 0) {
            LOG_ERROR("Texture failed to load: " << filePath);
            return nullptr;
        }

        // Successful: send success message
        Messaging::MessageSystem::Notify(Messaging::ResourceLoaded{ filePath, "Texture", true });
        return texture;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading texture " << filePath << ": " << e.what());
        return nullptr;
    }
}

// Loading function for audio
template<>std::shared_ptr<AudioData> ResourceManager::Load<AudioData>(const std::string& filePath) {
    // Check if the file actually exists before trying to open it
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    try {
        // Get and validate extension
        std::string extension = std::filesystem::path(filePath).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        // Just in case there's a leading dot
        if (!extension.empty() && extension[0] == '.') {
            extension = extension.substr(1);
        }

        // Validate format
        if (extension != "wav" && extension != "mp3" && extension != "ogg" &&
            extension != "flac" && extension != "m4a" && extension != "aac") {
            LOG_WARNING("Unsupported audio format: " << filePath);
            return nullptr;
        }

        // If file exists, then open in binary mode (because of audio files)
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file) {
            LOG_ERROR("Failed to open file: " << filePath);
            return nullptr;
        }

        // Find total file size
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Create audio object
        // make_shared is safer and won't leak memory
        auto audio = std::make_shared<AudioData>();
        audio->Path = filePath;
        audio->Format = extension;

        // Prepare vector for file data then read file into memory
        audio->Data.resize(fileSize);
        file.read(reinterpret_cast<char*>(audio->Data.data()), fileSize);

        // Checks
        if (!file) {
            LOG_ERROR("Failed to read file: " << filePath);
            return nullptr;
        }

        // Success
        audio->IsValid = true;
        return audio;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading audio " << filePath << ": " << e.what());
        return nullptr;
    }
}

// Loading function for fonts
template<>std::shared_ptr<Font> ResourceManager::Load<Font>(const std::string& filePath) {
    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    try {
        // Create font with default 48px size
        auto font = std::make_shared<Font>(filePath, 48);

        // Check if font loaded successfully
        if (font->getAtlasTexture() == 0) {
            LOG_ERROR("Font failed to load: " << filePath);
            return nullptr;
        }

        // Success
        return font;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading font " << filePath << ": " << e.what());
        return nullptr;
    }
}

// Loading function for raw data
template<>std::shared_ptr<RawData> ResourceManager::Load<RawData>(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    try {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file) {
            LOG_ERROR("Failed to open file: " << filePath);
            return nullptr;
        }

        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        auto raw = std::make_shared<RawData>();
        raw->Path = filePath;
        raw->Data.resize(fileSize);
        file.read(reinterpret_cast<char*>(raw->Data.data()), fileSize);

        if (!file) {
            LOG_ERROR("Failed to read file: " << filePath);
            return nullptr;
        }

        raw->IsValid = true;
        return raw;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading raw file " << filePath << ": " << e.what());
        return nullptr;
    }
}

// Loading function for shaders
template<>std::shared_ptr<Shader> ResourceManager::Load<Shader>(const std::string& key) {
    std::string vertPath;
    std::string fragPath;

    size_t separator = key.find('|');
    if (separator != std::string::npos) {
        vertPath = key.substr(0, separator);
        fragPath = key.substr(separator + 1);
    }
    else {
        vertPath = key + ".vert";
        fragPath = key + ".frag";
    }

    if (!std::filesystem::exists(vertPath)) {
        LOG_ERROR("Vertex shader not found: " << vertPath);
        return nullptr;
    }

    if (!std::filesystem::exists(fragPath)) {
        LOG_ERROR("Fragment shader not found: " << fragPath);
        return nullptr;
    }

    try {
        auto shader = std::make_shared<Shader>(vertPath, fragPath);
        LOG_INFO("Loaded shader: " << key);
        return shader;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading shader " << key << ": " << e.what());
        return nullptr;
    }
}

// Loading function for prefabs
template<>std::shared_ptr<PrefabData> ResourceManager::Load<PrefabData>(const std::string& filePath) {
    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
        LOG_ERROR("File not found: " << filePath);
        return nullptr;
    }

    // Validate extension
    std::string extension = std::filesystem::path(filePath).extension().string();
    if (extension != ".prefab") {
        LOG_WARNING("Not a .prefab file: " << filePath);
        return nullptr;
    }

    try {
        // Open file and read JSON content
        std::ifstream file(filePath);
        if (!file) {
            LOG_ERROR("Failed to open file: " << filePath);
            return nullptr;
        }

        // Read entire file into string
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonContent = buffer.str();

        // Create prefab data object
        auto prefab = std::make_shared<PrefabData>();
        prefab->Path = filePath;
        prefab->JsonContent = jsonContent;
        prefab->IsValid = true;

        // Success
        LOG_INFO("Loaded prefab: " << filePath);
        return prefab;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading prefab " << filePath << ": " << e.what());
        return nullptr;
    }
}

// Load font with custom pixel size
std::shared_ptr<Font> ResourceManager::GetFont(const std::string& name, int pixelSize) {
    // Create unique cache key that includes size (same font, different sizes cached separately)
    std::string cacheKey = name + ":" + std::to_string(pixelSize);

    // Check cache first
    auto it = m_fonts.find(cacheKey);
    if (it != m_fonts.end()) {
        if (m_loggedFontCacheHits.insert(cacheKey).second) {
            LOG_DEBUG("[CACHE HIT] Font: " << name << " (" << pixelSize << "px)");
        }
        TrackOwner<Font>(cacheKey);
        return it->second;  // Cache hit
    }

    // Not in cache: try to load
    LOG_DEBUG("[CACHE MISS] Loading font: " << name << " (" << pixelSize << "px)");

    // Check if the file actually exists before trying to open it
    if (!std::filesystem::exists(name)) {
        LOG_ERROR("File not found: " << name);
        return nullptr;
    }

    try {
        // Create font with specified pixel size
        auto font = std::make_shared<Font>(name, pixelSize);

        // Check if font loaded successfully
        if (font->getAtlasTexture() == 0) {
            LOG_ERROR("Font failed to load: " << name);
            return nullptr;
        }

        // Cache the loaded font
        m_fonts[cacheKey] = font;
        TrackOwner<Font>(cacheKey);
        LOG_DEBUG("Cached font: " << name << " (" << pixelSize << "px)");
        return font;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception loading font " << name << ": " << e.what());
        return nullptr;
    }
}

std::shared_ptr<Shader> ResourceManager::GetShader(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string key = vertexPath + "|" + fragmentPath;
    return Get<Shader>(key);
}

// Clear all cached assets
void ResourceManager::ClearCache() {
    m_textures.clear();
    m_audioFiles.clear();
    m_fonts.clear();
    m_shaders.clear();
    m_prefabs.clear();
    m_rawData.clear();
    m_textureOwners.clear();
    m_audioOwners.clear();
    m_fontOwners.clear();
    m_shaderOwners.clear();
    m_prefabOwners.clear();
    m_rawDataOwners.clear();
    m_loggedFontCacheHits.clear();
    LOG_INFO("Cleared all cached assets");
}

// Remove a specific asset from cache by name
void ResourceManager::UnloadAsset(const std::string& name) {
    bool removed = false;

    // Try to remove from texture cache
    // Equivalent to: removed = removed | (m_textures.erase(name) > 0);
    // Or: if (m_textures.erase(name) > 0) { removed = true; } i.e. keep true if already true
    removed |= m_textures.erase(name) > 0;

    // Try to remove from all caches
    removed |= m_audioFiles.erase(name) > 0;

    // Fonts use composite key (name:size), so we must iterate
    const std::string fontPrefix = name + ":";
    for (auto it = m_fonts.begin(); it != m_fonts.end(); ) {
        // Check if key starts with "name:"
        if (it->first.rfind(fontPrefix, 0) == 0) {
            m_loggedFontCacheHits.erase(it->first);
            it = m_fonts.erase(it);
            removed = true;
        }
        else {
            ++it;
        }
    }
    // Also try direct match (just in case)
    if (m_fonts.erase(name) > 0) {
        m_loggedFontCacheHits.erase(name);
        removed = true;
    }
    removed |= m_shaders.erase(name) > 0;
    removed |= m_prefabs.erase(name) > 0;
    removed |= m_rawData.erase(name) > 0;

    m_textureOwners.erase(name);
    m_audioOwners.erase(name);
    m_fontOwners.erase(name);
    m_shaderOwners.erase(name);
    m_prefabOwners.erase(name);
    m_rawDataOwners.erase(name);

    if (removed) {
        LOG_INFO("Unloaded asset: " << name);
    }
    else {
        LOG_WARNING("Asset not found in cache: " << name);
    }
}

// Get total number of cached assets
size_t ResourceManager::GetCacheSize() const {
    return m_textures.size() + m_audioFiles.size() + m_fonts.size() + m_shaders.size() + m_prefabs.size() + m_rawData.size();
}

// Get cache info broken down by type
void ResourceManager::PrintCacheInfo() const {
    LOG_INFO("\nCache Info:");
    LOG_INFO("Textures: " << m_textures.size());
    LOG_INFO("Audio files: " << m_audioFiles.size());
    LOG_INFO("Fonts: " << m_fonts.size());
    LOG_INFO("Shaders: " << m_shaders.size());
    LOG_INFO("Prefabs: " << m_prefabs.size());
    LOG_INFO("Raw data: " << m_rawData.size());
    LOG_INFO("Total assets: " << GetCacheSize());

    // Detailed owner info
    auto dumpOwners = [&](const char* label, const auto& ownerMap) {
        // Format:
        // <Label> Owners:
        //   OwnerTag1 -> N assets
        //   asset1
        //   asset2...
        LOG_INFO(std::string(label) + " Owners:");
        if (ownerMap.empty()) {
            LOG_INFO("  (none)");
            return;
        }

        // Invert map to group by owner tag
        std::unordered_map<std::string, std::vector<std::string>> ownersToAssets;
        for (const auto& [asset, owners] : ownerMap) {
            for (const auto& owner : owners) {
                ownersToAssets[owner].push_back(asset);
            }
        }

        // Print grouped info
        // Format:
        // OwnerTag -> N assets
        //   asset1
        //   asset2...
        for (const auto& [owner, assets] : ownersToAssets) {
            LOG_INFO("  " << owner << " -> " << assets.size() << " assets");
            for (const auto& asset : assets) {
                LOG_INFO("    " << asset);
            }
        }
    };

    // After that, call the helper for each asset type
    dumpOwners("Texture", m_textureOwners);
    dumpOwners("Audio", m_audioOwners);
    dumpOwners("Font", m_fontOwners);
    dumpOwners("Shader", m_shaderOwners);
    dumpOwners("Prefab", m_prefabOwners);
    dumpOwners("RawData", m_rawDataOwners);
}

// Get cached audio paths for Audio to load in library
std::vector<std::string> ResourceManager::ListCachedAudioPaths() const {
    std::vector<std::string> result;
    result.reserve(m_audioFiles.size());
    for (const auto& [path, audioPtr] : m_audioFiles) {
        if (audioPtr && audioPtr->IsValid) {
            result.push_back(path);
        }
    }
    return result;
}

// Check if a specific asset is cached
bool ResourceManager::IsAssetCached(const std::string& name) const {
    return (m_textures.find(name) != m_textures.end()) ||
        (m_audioFiles.find(name) != m_audioFiles.end()) ||
        (m_fonts.find(name) != m_fonts.end()) ||
        (m_shaders.find(name) != m_shaders.end()) ||
        (m_prefabs.find(name) != m_prefabs.end()) ||
        (m_rawData.find(name) != m_rawData.end());
}

// Import/add an asset file to the project
bool ResourceManager::AddAsset(const std::string& sourcePath, const std::string& destPath) {
    try {
        // Validate source file/directory exists
        if (!std::filesystem::exists(sourcePath)) {
            LOG_ERROR("Source file not found: " << sourcePath);
            return false;
        }

        // Create destination directories if needed
        std::filesystem::path destPathObj(destPath);
        if (destPathObj.has_parent_path()) {
            std::filesystem::create_directories(destPathObj.parent_path());
        }

        // Handle directories vs files
        if (std::filesystem::is_directory(sourcePath)) {
            // Copy entire directory recursively
            std::filesystem::copy(sourcePath, destPath,
                std::filesystem::copy_options::overwrite_existing |
                std::filesystem::copy_options::recursive);
        }
        else {
            // Copy single file
            std::filesystem::copy_file(sourcePath, destPath,
                std::filesystem::copy_options::overwrite_existing);
        }

        LOG_INFO("Added asset: " << destPath);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to add asset " << destPath << ": " << e.what());
        return false;
    }
}

// Delete an asset file from the project
bool ResourceManager::DeleteAsset(const std::string& assetPath) {
    try {
        // Remove from cache first
        UnloadAsset(assetPath);

        // Check if file/folder exists
        if (!std::filesystem::exists(assetPath)) {
            LOG_WARNING("Asset not found: " << assetPath);
            return false;
        }

        // Delete from filesystem
        if (std::filesystem::is_directory(assetPath)) {
            std::filesystem::remove_all(assetPath);
        }
        else {
            std::filesystem::remove(assetPath);
        }

        LOG_INFO("Deleted asset: " << assetPath);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to delete asset " << assetPath << ": " << e.what());
        return false;
    }
}

// Replace an existing asset with a new file
bool ResourceManager::ReplaceAsset(const std::string& assetPath, const std::string& newSourcePath) {
    try {
        // Validate source file exists
        if (!std::filesystem::exists(newSourcePath)) {
            LOG_ERROR("Source file not found: " << newSourcePath);
            return false;
        }

        // Unload from cache before replacing
        UnloadAsset(assetPath);

        // Replace file
        std::filesystem::copy_file(newSourcePath, assetPath,
            std::filesystem::copy_options::overwrite_existing);

        // Reload into cache (determine type by extension)
        std::string ext = std::filesystem::path(assetPath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
            Get<Texture>(assetPath);
        }
        else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".m4a" || ext == ".aac") {
            Get<AudioData>(assetPath);
        }
        else if (ext == ".ttf" || ext == ".otf") {
            Get<Font>(assetPath);
        }
        else if (ext == ".prefab") {
            Get<PrefabData>(assetPath);
        }

        LOG_INFO("Replaced asset: " << assetPath);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to replace asset " << assetPath << ": " << e.what());
        return false;
    }
}

// Create a new empty directory
bool ResourceManager::CreateDirectory(const std::string& path) {
    try {
        if (std::filesystem::exists(path)) {
            LOG_WARNING("Directory already exists: " << path);
            return true; // Not an error if it already exists
        }

        std::filesystem::create_directories(path);
        LOG_INFO("Created directory: " << path);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to create directory " << path << ": " << e.what());
        return false;
    }
}

// Set the current owner tag for asset tracking
void ResourceManager::SetOwnerTag(const std::string& ownerTag) {
    m_ownerTag = ownerTag;
}

// Clear the current owner tag
void ResourceManager::ClearOwnerTag() {
    m_ownerTag.clear();
}

// Unload assets that are only owned by the specified tag
void ResourceManager::UnloadAssetsByOwner(const std::string& ownerTag) {
    if (ownerTag.empty()) {
        return;
    }

    // Helper lambda to remove assets owned by ownerTag from given ownerMap and cacheMap
    // This avoids code duplication for each asset type
    auto removeOwner = [&](auto& ownerMap, auto& cacheMap) {
        for (auto it = ownerMap.begin(); it != ownerMap.end(); ) {
            auto& owners = it->second; // set of owner tags for this asset
            owners.erase(ownerTag); // remove the specified owner tag

            // If no more owners, remove from cache and owner map
            if (owners.empty()) {
                cacheMap.erase(it->first);
                it = ownerMap.erase(it);
            } else {
                ++it;
            }
        }
    };

    // After that, call the helper for each asset type
    removeOwner(m_textureOwners, m_textures);
    removeOwner(m_audioOwners, m_audioFiles);
    removeOwner(m_fontOwners, m_fonts);
    removeOwner(m_shaderOwners, m_shaders);
    removeOwner(m_prefabOwners, m_prefabs);
    removeOwner(m_rawDataOwners, m_rawData);

	// Clean up logged font cache hits to remove any fonts that were unloaded
    for (auto it = m_loggedFontCacheHits.begin(); it != m_loggedFontCacheHits.end(); ) {
        if (m_fonts.find(*it) == m_fonts.end()) {
            it = m_loggedFontCacheHits.erase(it);
        }
        else {
            ++it;
        }
    }
}

// Define the global ResourceManager instance
ResourceManager RM;
