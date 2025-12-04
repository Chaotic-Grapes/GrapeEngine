
#ifndef AUDIODIAGNOSTICS_H
#define AUDIODIAGNOSTICS_H

#include "scene/Scene.h"
#include "core/Application.h"
#include "core/Logger.h"
#include "AudioAssetLibrary.h"
#include "ecs/systems/AudioSystem.h"

namespace AudioDiagnostics {

    /**
     * @brief Comprehensive diagnostic check for audio system configuration
     * @param scene The scene to diagnose (usually the active scene)
     * @return True if all checks pass, false if issues found
     */
    inline bool DiagnoseAudioSystem(Scenes::Scene* scene) {
        if (!scene) {
            LOG_ERROR("=== AUDIO DIAGNOSIS: FAILED ===");
            LOG_ERROR("No active scene");
            return false;
        }

        bool allChecksPass = true;

        LOG_INFO("=== AUDIO SYSTEM DIAGNOSIS START ===");

        // ----------------------------------------------------------------
        // Check 1: Is Audio system registered in SystemManager?
        // ----------------------------------------------------------------
        auto& systemManager = Engine::CORE->GetSystemManager();
        auto* audioSystem = systemManager.GetSystem<ECS::AudioSystem>();
        if (!audioSystem) {
            LOG_ERROR("FAILED: Audio system NOT REGISTERED in SystemManager");
            LOG_ERROR("       -> Register it in Application::_registerSystems()");
            allChecksPass = false;
        }
        else {
            LOG_INFO("PASS: Audio system is registered in SystemManager");
            
            // Check if enabled
            if (audioSystem->IsEnabled()) {
                LOG_INFO("PASS: Audio system is ENABLED");
            }
            else {
                LOG_WARNING("WARNING: Audio system is DISABLED");
                LOG_INFO("         -> This is normal in editor edit mode");
            }
        }

        // ----------------------------------------------------------------
        // Check 3: Are there entities with AudioSource components?
        // ----------------------------------------------------------------
        int audioSourceCount = 0;
        int validCueCount = 0;
        auto& lib = AudioAssetLibrary::Get();

        scene->GetWorld().Each<ECS::Components::AudioSource>(
            [&](ECS::Entity e, const ECS::Components::AudioSource& src) {
                audioSourceCount++;

                std::string entityName = "Unknown";
                if (scene->GetWorld().Has<ECS::Components::Name>(e)) {
                    entityName = scene->GetWorld().Get<ECS::Components::Name>(e).Value;
                }

                if (src.CueId == 0) {
                    LOG_WARNING("  Entity " << e.Index << " (" << entityName << "): AudioSource has CueId=0 (no sound assigned)");
                }
                else {
                    const auto* clip = lib.FindById(src.CueId);
                    if (clip) {
                        LOG_INFO("  Entity " << e.Index << " (" << entityName << "): CueId=" << src.CueId
                            << " -> " << clip->path
                            << " (vol=" << src.Volume << ", pitch=" << src.Pitch << ", loop=" << src.Loop << ")");
                        validCueCount++;
                    }
                    else {
                        LOG_ERROR("  Entity " << e.Index << " (" << entityName << "): CueId=" << src.CueId
                            << " NOT FOUND in AudioAssetLibrary!");
                        allChecksPass = false;
                    }
                }
            });

        if (audioSourceCount == 0) {
            LOG_WARNING("WARNING: No entities with AudioSource component found in scene");
            LOG_INFO("         -> Add AudioSource components to entities that should play sounds");
        }
        else {
            LOG_INFO("Found " << audioSourceCount << " entities with AudioSource ("
                << validCueCount << " with valid cues)");
        }

        // ----------------------------------------------------------------
        // Check 4: Is AudioService available and initialized?
        // ----------------------------------------------------------------
        auto* app = Engine::CORE;
        auto* audioSvc = app ? app->GetAudioService() : nullptr;
        if (!audioSvc) {
            LOG_ERROR("FAILED: AudioService not available (Engine::CORE->GetAudioService() returned nullptr)");
            LOG_ERROR("       -> Ensure AudioService is initialized in Application::_initializeServices()");
            allChecksPass = false;
        }
        else {
            LOG_INFO("PASS: AudioService is available");

            auto* device = audioSvc->Device();
            if (!device) {
                LOG_ERROR("FAILED: AudioService has no device (Device() returned nullptr)");
                LOG_ERROR("       -> Check AudioService::Initialize() and FmodAudioDevice::Initialize()");
                allChecksPass = false;
            }
            else {
                LOG_INFO("PASS: FmodAudioDevice is available");

                // Check loaded cues
                std::vector<std::pair<std::string, std::string>> loadedCues;
                device->GetLoadedCues(loadedCues);
                if (loadedCues.empty()) {
                    LOG_INFO("Note: No audio cues loaded yet (will be loaded on first play)");
                }
                else {
                    LOG_INFO("Currently loaded cues: " << loadedCues.size());
                    for (const auto& [id, path] : loadedCues) {
                        LOG_DEBUG("  " << id << " -> " << path);
                    }
                }
            }
        }

        // ----------------------------------------------------------------
        // Check 5: Check AudioAssetLibrary
        // ----------------------------------------------------------------
        int totalClips = 0;
        // Note: You'd need to add a method to AudioAssetLibrary to get count
        // For now just log that we're using it
        LOG_INFO("AudioAssetLibrary is being used for CueId -> path resolution");

        // ----------------------------------------------------------------
        // Summary
        // ----------------------------------------------------------------
        LOG_INFO("=== AUDIO SYSTEM DIAGNOSIS " << (allChecksPass ? "COMPLETE" : "FOUND ISSUES") << " ===");

        if (!allChecksPass) {
            LOG_INFO("\n=== QUICK FIX CHECKLIST ===");
            LOG_INFO("1. Make sure Audio system is registered in Application.cpp");
            LOG_INFO("2. Verify scene SystemProfile includes Audio system (check .scene file)");
            LOG_INFO("3. Ensure entities have AudioSource with valid CueIds");
            LOG_INFO("4. Check AudioService is initialized in Application");
            LOG_INFO("5. Verify audio files exist at the paths in AudioAssetLibrary");
            LOG_INFO("===========================\n");
        }

        return allChecksPass;
    }

    /**
     * @brief Quick check if audio can play for a specific entity
     * @param scene The scene containing the entity
     * @param entityId The entity to check
     * @return True if entity can play audio, false otherwise
     */
    inline bool CanEntityPlayAudio(Scenes::Scene* scene, uint32_t entityId) {
        if (!scene) return false;

        auto& world = scene->GetWorld();
        ECS::Entity e = world.Resolve(entityId);

        if (!world.IsAlive(e)) {
            LOG_ERROR("Entity " << entityId << " is not alive");
            return false;
        }

        if (!world.Has<ECS::Components::AudioSource>(e)) {
            LOG_ERROR("Entity " << entityId << " has no AudioSource component");
            return false;
        }

        auto& src = world.Get<ECS::Components::AudioSource>(e);
        if (src.CueId == 0) {
            LOG_ERROR("Entity " << entityId << " AudioSource has no CueId set");
            return false;
        }

        auto& lib = AudioAssetLibrary::Get();
        const auto* clip = lib.FindById(src.CueId);
        if (!clip) {
            LOG_ERROR("Entity " << entityId << " CueId " << src.CueId << " not found in library");
            return false;
        }

        LOG_INFO("Entity " << entityId << " can play audio: " << clip->path);
        return true;
    }

} // namespace AudioDiagnostics

#endif // AUDIODIAGNOSTICS_H
