/* Start Header *****************************************************************/
/*!
\file   DeviceManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Provides hardware device enumeration and runtime device management for
display/monitor and audio devices. Enables users to query available
resolutions, refresh rates, audio devices, and detect device changes.

Features:
- Monitor enumeration with display mode querying
- Audio device enumeration (output/input)
- Real-time device change detection
- Support for device switching at runtime

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "Export.h"
#include <vector>
#include <string>
#include <cstdint>

namespace Engine {

    // ==================== Display/Monitor Info ====================
    
    /**
     * @brief Represents a display mode (resolution + refresh rate)
     */
    struct GRAPEENGINE_API DisplayMode {
        int32_t Width;              // Horizontal resolution in pixels
        int32_t Height;             // Vertical resolution in pixels
        int32_t RefreshRate;        // Refresh rate in Hz
        uint8_t BitsPerPixel;       // Color depth (e.g., 32)

        /**
         * @brief Convert to string representation
         */
        std::string ToString() const;

        /**
         * @brief Equality comparison
         */
        bool operator==(const DisplayMode& other) const;
    };

    /**
     * @brief Information about a connected monitor/display
     */
    struct GRAPEENGINE_API MonitorInfo {
        std::string Name;                           // Monitor name
        std::vector<DisplayMode> SupportedModes;    // Available display modes
        DisplayMode CurrentMode;                    // Currently active mode
        int32_t PositionX;                          // X position in virtual desktop
        int32_t PositionY;                          // Y position in virtual desktop
        int32_t WidthMM;                            // Physical width in millimeters
        int32_t HeightMM;                           // Physical height in millimeters
        bool IsPrimary;                             // Is this the primary monitor

        /**
         * @brief Calculate DPI (dots per inch)
         */
        float GetDPI() const;

        /**
         * @brief Get aspect ratio
         */
        float GetAspectRatio() const;
    };

    // ==================== Audio Device Info ====================
    
    /**
     * @brief Information about an audio device
     */
    struct GRAPEENGINE_API AudioDeviceInfo {
        std::string DeviceID;           // Unique device identifier
        std::string DeviceName;         // Human-readable device name
        int32_t Channels;               // Number of audio channels
        int32_t SampleRate;             // Sample rate in Hz
        bool IsDefault;                 // Is this the system default device
        bool IsConnected;               // Is device currently connected

        /**
         * @brief Convert to string representation
         */
        std::string ToString() const;

        /**
         * @brief Equality comparison
         */
        bool operator==(const AudioDeviceInfo& other) const;
    };

    // ==================== DeviceManager ====================
    
    /**
     * @brief Manages enumeration and discovery of hardware devices
     * 
     * Static utility class providing queries for:
     * - Display/Monitor capabilities
     * - Audio device enumeration
     * - Device change detection
     * 
     * Example usage:
     * @code
     * // Get available monitors and their display modes
     * auto monitors = Engine::DeviceManager::EnumerateMonitors();
     * for (const auto& monitor : monitors) {
     *     LOG_INFO("Monitor: " << monitor.Name);
     *     for (const auto& mode : monitor.SupportedModes) {
     *         LOG_INFO("  Mode: " << mode.ToString());
     *     }
     * }
     * 
     * // Check audio devices
     * auto audioDevices = Engine::DeviceManager::EnumerateAudioOutputDevices();
     * auto defaultDevice = Engine::DeviceManager::GetDefaultAudioOutputDevice();
     * @endcode
     */
    class GRAPEENGINE_API DeviceManager {
    public:
        // ==================== Display/Monitor Queries ====================
        
        /**
         * @brief Enumerate all connected monitors
         * @return Vector of MonitorInfo for each connected monitor
         */
        static std::vector<MonitorInfo> EnumerateMonitors();

        /**
         * @brief Get information about the primary monitor
         * @return MonitorInfo for the primary display, or empty if none found
         */
        static MonitorInfo GetPrimaryMonitor();

        /**
         * @brief Get all supported display modes for a monitor
         * @param monitorIndex Index of the monitor (0 = primary)
         * @return Vector of DisplayMode options available for the monitor
         */
        static std::vector<DisplayMode> GetDisplayModes(int32_t monitorIndex);

        /**
         * @brief Get display modes for a named monitor
         * @param monitorName Name of the monitor
         * @return Vector of DisplayMode options, empty if monitor not found
         */
        static std::vector<DisplayMode> GetDisplayModesByName(const std::string& monitorName);

        /**
         * @brief Get the total number of connected monitors
         * @return Count of monitors
         */
        static int32_t GetMonitorCount();

        // ==================== Audio Device Queries ====================
        
        /**
         * @brief Enumerate all connected audio output devices
         * @return Vector of AudioDeviceInfo for output devices
         */
        static std::vector<AudioDeviceInfo> EnumerateAudioOutputDevices();

        /**
         * @brief Enumerate all connected audio input devices
         * @return Vector of AudioDeviceInfo for input devices
         */
        static std::vector<AudioDeviceInfo> EnumerateAudioInputDevices();

        /**
         * @brief Get the system default audio output device
         * @return AudioDeviceInfo for the default output device
         */
        static AudioDeviceInfo GetDefaultAudioOutputDevice();

        /**
         * @brief Get the system default audio input device
         * @return AudioDeviceInfo for the default input device
         */
        static AudioDeviceInfo GetDefaultAudioInputDevice();

        /**
         * @brief Get information about a specific audio device
         * @param deviceID Unique device identifier
         * @return AudioDeviceInfo if found, default-constructed struct if not
         */
        static AudioDeviceInfo GetAudioDeviceInfo(const std::string& deviceID);

        // ==================== Device Change Detection ====================
        
        /**
         * @brief Start monitoring for device changes (audio plug/unplug)
         * 
         * Should be called once at engine initialization.
         * Call HasAudioDevicesChanged() each frame to check for changes.
         */
        static void StartAudioDeviceChangeDetection();

        /**
         * @brief Stop monitoring for device changes
         * 
         * Should be called during shutdown.
         */
        static void StopAudioDeviceChangeDetection();

        /**
         * @brief Check if audio devices have changed since last call
         * 
         * @return true if devices were plugged/unplugged/default changed
         * 
         * After returning true, this returns false until devices change again.
         * Useful in Application::Update() to detect and respond to changes.
         */
        static bool HasAudioDevicesChanged();

    private:
        // Private constructor - only static methods
        DeviceManager() = delete;
    };

}

#endif
