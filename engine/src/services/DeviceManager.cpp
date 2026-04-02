/* Start Header *****************************************************************/
/*!
\file    DeviceManager.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implements hardware device enumeration using GLFW (display) and FMOD (audio).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "services/DeviceManager.h"
#include "core/Logger.h"
#include <GLFW/glfw3.h>
#include <fmod.hpp>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace Engine {

    // ==================== Static State ====================
    
    namespace {
        FMOD::System* g_fmodSystem = nullptr;
        bool g_deviceChangeDetectionActive = false;
        bool g_audioDevicesChanged = false;
        int32_t g_lastAudioDeviceCount = 0;
        std::string g_lastDefaultAudioDevice;
        
        // Audio device caching
        std::vector<AudioDeviceInfo> g_cachedOutputDevices;
        std::vector<AudioDeviceInfo> g_cachedInputDevices;
        bool g_outputDevicesCached = false;
        bool g_inputDevicesCached = false;
    }

    // ==================== DisplayMode Implementation ====================
    
    std::string DisplayMode::ToString() const {
        std::ostringstream oss;
        oss << Width << "x" << Height << " @ " << RefreshRate << "Hz (" << static_cast<int>(BitsPerPixel) << "-bit)";
        return oss.str();
    }

    bool DisplayMode::operator==(const DisplayMode& other) const {
        return Width == other.Width && Height == other.Height && RefreshRate == other.RefreshRate;
    }

    // ==================== MonitorInfo Implementation ====================
    
    float MonitorInfo::GetDPI() const {
        if (WidthMM <= 0) return 96.0f;  // Default DPI
        // DPI = (pixels * 25.4) / millimeters
        return (CurrentMode.Width * 25.4f) / WidthMM;
    }

    float MonitorInfo::GetAspectRatio() const {
        if (CurrentMode.Height == 0) return 16.0f / 9.0f;
        return static_cast<float>(CurrentMode.Width) / CurrentMode.Height;
    }

    // ==================== AudioDeviceInfo Implementation ====================
    
    std::string AudioDeviceInfo::ToString() const {
        std::ostringstream oss;
        oss << DeviceName << " (" << Channels << "ch @ " << SampleRate << "Hz)";
        if (IsDefault) oss << " [DEFAULT]";
        if (!IsConnected) oss << " [DISCONNECTED]";
        return oss.str();
    }

    bool AudioDeviceInfo::operator==(const AudioDeviceInfo& other) const {
        return DeviceID == other.DeviceID;
    }

    // ==================== DeviceManager - Display Implementation ====================
    
    /**
     * @brief Enumerates all connected monitors and their display modes.
     * @return A vector containing one MonitorInfo entry per connected monitor.
     * @note GLFW can return partial monitor metadata on some driver/OS combinations,
     *       so this function applies safe defaults when metadata is unavailable.
     * @complexity O(M + K) where M is monitor count and K is total mode count.
     */
    std::vector<MonitorInfo> DeviceManager::EnumerateMonitors() {
        std::vector<MonitorInfo> monitors;

        int monitorCount = 0;
        GLFWmonitor** glfwMonitors = glfwGetMonitors(&monitorCount);

        if (!glfwMonitors || monitorCount == 0) {
            LOG_WARNING("No monitors found");
            return monitors;
        }

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();

        for (int i = 0; i < monitorCount; ++i) {
            GLFWmonitor* glfwMonitor = glfwMonitors[i];
            MonitorInfo info{};

            // Get basic info
            const char* monitorName = glfwGetMonitorName(glfwMonitor);
            info.Name = (monitorName != nullptr) ? monitorName : "Unknown Monitor";
            info.IsPrimary = (primaryMonitor != nullptr) ? (glfwMonitor == primaryMonitor) : (i == 0);

            // Get position
            glfwGetMonitorPos(glfwMonitor, &info.PositionX, &info.PositionY);

            // Get physical size
            glfwGetMonitorPhysicalSize(glfwMonitor, &info.WidthMM, &info.HeightMM);

            // Get current mode
            const GLFWvidmode* currentMode = glfwGetVideoMode(glfwMonitor);
            if (currentMode) {
                info.CurrentMode.Width = currentMode->width;
                info.CurrentMode.Height = currentMode->height;
                info.CurrentMode.RefreshRate = currentMode->refreshRate;
                info.CurrentMode.BitsPerPixel = static_cast<uint8_t>(
                    currentMode->redBits + currentMode->greenBits + currentMode->blueBits
                );
            } else {
                LOG_WARNING("[DeviceManager] Failed to read current mode for monitor: " << info.Name);
            }

            // Get available modes
            int modeCount = 0;
            const GLFWvidmode* glfwModes = glfwGetVideoModes(glfwMonitor, &modeCount);

            if (glfwModes && modeCount > 0) {
                for (int j = 0; j < modeCount; ++j) {
                    DisplayMode mode;
                    mode.Width = glfwModes[j].width;
                    mode.Height = glfwModes[j].height;
                    mode.RefreshRate = glfwModes[j].refreshRate;
                    mode.BitsPerPixel = static_cast<uint8_t>(
                        glfwModes[j].redBits + glfwModes[j].greenBits + glfwModes[j].blueBits
                    );

                    // Avoid duplicates
                    if (std::find(info.SupportedModes.begin(), info.SupportedModes.end(), mode) 
                        == info.SupportedModes.end()) {
                        info.SupportedModes.push_back(mode);
                    }
                }
            }

            monitors.push_back(info);
            LOG_INFO("Monitor found: " << info.Name << " (" << info.CurrentMode.ToString() << ")");
        }

        return monitors;
    }

    MonitorInfo DeviceManager::GetPrimaryMonitor() {
        auto monitors = EnumerateMonitors();
        if (!monitors.empty()) {
            return monitors[0];
        }
        return MonitorInfo{};
    }

    std::vector<DisplayMode> DeviceManager::GetDisplayModes(int32_t monitorIndex) {
        auto monitors = EnumerateMonitors();
        if (monitorIndex >= 0 && monitorIndex < static_cast<int32_t>(monitors.size())) {
            return monitors[monitorIndex].SupportedModes;
        }
        return {};
    }

    std::vector<DisplayMode> DeviceManager::GetDisplayModesByName(const std::string& monitorName) {
        auto monitors = EnumerateMonitors();
        for (const auto& monitor : monitors) {
            if (monitor.Name == monitorName) {
                return monitor.SupportedModes;
            }
        }
        return {};
    }

    int32_t DeviceManager::GetMonitorCount() {
        int count = 0;
        glfwGetMonitors(&count);
        return count;
    }

    // ==================== DeviceManager - Audio Implementation ====================
    
    // Helper: Get or initialize FMOD system
    static FMOD::System* GetFmodSystem() {
        if (g_fmodSystem) {
            return g_fmodSystem;
        }

        // Try to get from AudioService if available
        // For now, we'll create/manage our own instance for device enumeration
        FMOD::System_Create(&g_fmodSystem);
        if (g_fmodSystem) {
            unsigned int version = 0;
            g_fmodSystem->getVersion(&version);

            int numDrivers = 0;
            g_fmodSystem->getNumDrivers(&numDrivers);
            
            if (numDrivers > 0) {
                // Initialize with dummy buffer just for enumeration
                g_fmodSystem->init(numDrivers, FMOD_INIT_NORMAL, nullptr);
            }
        }
        return g_fmodSystem;
    }

    std::vector<AudioDeviceInfo> DeviceManager::EnumerateAudioOutputDevices() {
        // Return cached result if available and devices haven't changed
        if (g_outputDevicesCached && (!g_deviceChangeDetectionActive || !g_audioDevicesChanged)) {
            return g_cachedOutputDevices;
        }

        std::vector<AudioDeviceInfo> devices;
        FMOD::System* system = GetFmodSystem();

        if (!system) {
            LOG_ERROR("Failed to initialize FMOD for audio device enumeration");
            return devices;
        }

        int numDrivers = 0;
        system->getNumDrivers(&numDrivers);

        int defaultDriver = 0;
        system->getDriver(&defaultDriver);

        for (int i = 0; i < numDrivers; ++i) {
            char name[256] = {};
            int systemRate = 0;
            FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_DEFAULT;
            int numRawSpeakers = 0;

            FMOD_RESULT result = system->getDriverInfo(
                i, name, sizeof(name), nullptr,
                &systemRate, &speakerMode, &numRawSpeakers
            );

            if (result == FMOD_OK) {
                AudioDeviceInfo info;
                info.DeviceName = name;
                
                // Create unique ID from index and name
                char idStr[256] = {};
                snprintf(idStr, sizeof(idStr), "output_%d_%s", i, name);
                info.DeviceID = idStr;

                info.SampleRate = systemRate;
                info.Channels = numRawSpeakers;
                info.IsDefault = (i == defaultDriver);
                info.IsConnected = true;

                devices.push_back(info);
                LOG_INFO("Audio output device: " << info.ToString());
            }
        }

        // Cache the results
        g_cachedOutputDevices = devices;
        g_outputDevicesCached = true;
        g_audioDevicesChanged = false;  // Clear the change flag after re-enumeration

        return devices;
    }

    std::vector<AudioDeviceInfo> DeviceManager::EnumerateAudioInputDevices() {
        // Return cached result if available and devices haven't changed
        if (g_inputDevicesCached && (!g_deviceChangeDetectionActive || !g_audioDevicesChanged)) {
            return g_cachedInputDevices;
        }

        std::vector<AudioDeviceInfo> devices;
        FMOD::System* system = GetFmodSystem();

        if (!system) {
            LOG_ERROR("Failed to initialize FMOD for audio device enumeration");
            return devices;
        }

        // FMOD's input device enumeration is complex and platform-dependent
        // For basic implementation, we return an empty list or a default mic
        // This can be extended per-platform in the future
        
        AudioDeviceInfo defaultMic;
        defaultMic.DeviceID = "input_default";
        defaultMic.DeviceName = "Default Microphone";
        defaultMic.SampleRate = 48000;
        defaultMic.Channels = 1;
        defaultMic.IsDefault = true;
        defaultMic.IsConnected = true;
        
        devices.push_back(defaultMic);
        LOG_INFO("Audio input device: " << defaultMic.ToString());

        // Cache the results
        g_cachedInputDevices = devices;
        g_inputDevicesCached = true;
        g_audioDevicesChanged = false;  // Clear the change flag after re-enumeration

        return devices;
    }

    AudioDeviceInfo DeviceManager::GetDefaultAudioOutputDevice() {
        auto devices = EnumerateAudioOutputDevices();
        for (const auto& device : devices) {
            if (device.IsDefault) {
                return device;
            }
        }

        // Fallback to first device if no default found
        if (!devices.empty()) {
            return devices[0];
        }

        return AudioDeviceInfo{};
    }

    AudioDeviceInfo DeviceManager::GetDefaultAudioInputDevice() {
        auto devices = EnumerateAudioInputDevices();
        for (const auto& device : devices) {
            if (device.IsDefault) {
                return device;
            }
        }

        // Fallback to first device
        if (!devices.empty()) {
            return devices[0];
        }

        return AudioDeviceInfo{};
    }

    AudioDeviceInfo DeviceManager::GetAudioDeviceInfo(const std::string& deviceID) {
        auto outputDevices = EnumerateAudioOutputDevices();
        for (const auto& device : outputDevices) {
            if (device.DeviceID == deviceID) {
                return device;
            }
        }

        auto inputDevices = EnumerateAudioInputDevices();
        for (const auto& device : inputDevices) {
            if (device.DeviceID == deviceID) {
                return device;
            }
        }

        return AudioDeviceInfo{};
    }

    // ==================== Device Change Detection ====================
    
    void DeviceManager::StartAudioDeviceChangeDetection() {
        g_deviceChangeDetectionActive = true;
        auto devices = EnumerateAudioOutputDevices();
        g_lastAudioDeviceCount = static_cast<int32_t>(devices.size());
        
        auto defaultDevice = GetDefaultAudioOutputDevice();
        g_lastDefaultAudioDevice = defaultDevice.DeviceID;

        LOG_INFO("Audio device change detection started");
    }

    void DeviceManager::StopAudioDeviceChangeDetection() {
        g_deviceChangeDetectionActive = false;
        LOG_INFO("Audio device change detection stopped");
    }

    bool DeviceManager::HasAudioDevicesChanged() {
        if (!g_deviceChangeDetectionActive) {
            return false;
        }

        auto devices = EnumerateAudioOutputDevices();
        int32_t currentCount = static_cast<int32_t>(devices.size());
        
        // Check if device count changed
        if (currentCount != g_lastAudioDeviceCount) {
            g_lastAudioDeviceCount = currentCount;
            g_audioDevicesChanged = true;
            LOG_WARNING("Audio device count changed: " << currentCount);
            return true;
        }

        // Check if default device changed
        auto defaultDevice = GetDefaultAudioOutputDevice();
        if (defaultDevice.DeviceID != g_lastDefaultAudioDevice) {
            g_lastDefaultAudioDevice = defaultDevice.DeviceID;
            g_audioDevicesChanged = true;
            LOG_WARNING("Default audio device changed to: " << defaultDevice.DeviceName);
            return true;
        }

        return g_audioDevicesChanged;
    }

}
