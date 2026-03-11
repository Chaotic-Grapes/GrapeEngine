/* Start Header *****************************************************************/
/*!
\file   ScriptsCompiler.cpp
\author Muhammad Nur Fadzly Bin Zulkifli
\date   26th January 2026
\brief
Implementation of the ScriptsCompiler which manages C# script compilation
and hot-reload orchestration used by the editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "scripting/ScriptsCompiler.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "core/Logger.h"
#include "EditorComponentRegistry.h"
#include "scripting/ScriptManager.h"

extern "C" {
    void RegisterComponentDeserializeCallback(void(*callback)(uint32_t, void*, int, const char*));
}

using namespace std::chrono_literals;

ScriptsCompiler::ScriptsCompiler(Engine::Application* engine, ECS::World* emptyWorld)
    : m_engine(engine), m_emptyWorld(emptyWorld) {
}

ScriptsCompiler::~ScriptsCompiler() {
    Shutdown();
}

void ScriptsCompiler::Initialize(ECS::ScriptManager* scriptManager) {
    m_scriptManager = scriptManager;
    m_shutdownRequested.store(false);
    if (m_scriptManager) {
        _registerCallbacks();
    }
}

void ScriptsCompiler::Start() {
    if (!m_scriptManager || !m_scriptManager->IsInitialized()) return;

    if (!Engine::ProjectPaths::IsInitialized()) {
        LOG_WARNING("[ScriptsCompiler] ProjectPaths not initialized, cannot initialize script compilation");
        return;
    }

    std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
    std::filesystem::path tempRoot = Engine::ProjectPaths::GetTempScriptsPath();
    std::filesystem::path scriptsOutput = Engine::ProjectPaths::GetCompiledScriptAssemblyPath();
    m_pendingScriptsOutput = scriptsOutput.string();

    // Generate the C# project file if available
    auto generateCsProj = m_scriptManager->GetGenerateCsProj();
    if (generateCsProj) {
        std::string csprojDir = Engine::ProjectPaths::GetCsProjPath();
        std::string scriptsRootStr = projectRoot.string();
        std::string projectName = std::filesystem::path(projectRoot).filename().string();
        int result = generateCsProj(csprojDir.c_str(), scriptsRootStr.c_str(), projectName.c_str());
        if (result == 0) {
            LOG_INFO("[ScriptsCompiler] Generated C# project file at: " << csprojDir << "/" << projectName << ".csproj");
        }
    }

    // Copy GrapeEngine.Scripting dependency to output dir
    m_scriptManager->CopyScriptingAssemblyToDirectory(scriptsOutput.parent_path().string());

    // Initial compilation on background thread
    LOG_INFO("[ScriptsCompiler] Scheduling background C# script compilation...");

    m_bgProgressThread = std::thread([this]() {
        while (!m_compileDone.load()) {
            int status = 0;
            int progress = -1;
            std::string message;
            m_scriptManager->GetCompileStatus(status, progress, message);
            if (status == 3 || status == 4) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }

        int status = 0, progress = -1;
        std::string message;
        m_scriptManager->GetCompileStatus(status, progress, message);
        LOG_INFO("[ScriptsCompiler] Script compile finished: status=" << status << " msg=" << message);
    });

    m_initialCompileThread = std::thread([this, scriptsOutput, projectRoot]() mutable {
        std::string diagnostics;
        try {
            m_scriptManager->SetCompileStatus(1, 0, "Starting compilation");
            LOG_INFO("[ScriptsCompiler] Background script compilation started");

            bool compileSuccess = _doCompileScripts(projectRoot.string(), scriptsOutput.string(), diagnostics);

            if (compileSuccess) {
                m_scriptManager->SetCompileStatus(3, 100, "Compilation successful");
                LOG_INFO("[ScriptsCompiler] Initial script compilation succeeded at: " << scriptsOutput.string());
                _doLoadAndRegisterSystems(scriptsOutput.string());
            } else {
                m_scriptManager->SetCompileStatus(4, 100, diagnostics.c_str());
            }

            if (m_scriptManager->StartScriptWatching(projectRoot.string())) {
                LOG_INFO("[ScriptsCompiler] C# script watcher started at: " << projectRoot.string());
            } else {
                LOG_WARNING("[ScriptsCompiler] Failed to start C# script watcher");
            }
        }
        catch (const std::exception& e) {
            m_scriptManager->SetCompileStatus(4, 100, e.what());
            LOG_ERROR("[ScriptsCompiler] Exception during script compilation: " << e.what());
        }
        m_compileDone.store(true);
    });
}

void ScriptsCompiler::_registerCallbacks() {
    if (!m_scriptManager) return;

    m_scriptManager->SetHotReloadCallback([this](const std::string& assemblyPath) {
        // C++ is the hot-reload authority.
        // C# callback is informational only.
        LOG_INFO("[ScriptsCompiler] C# hot reload notification received: " << assemblyPath);
    });

    m_scriptManager->SetScriptChangedCallback([this](const std::string& scriptsDir) {
        LOG_INFO("[ScriptsCompiler] Script changed callback: " << scriptsDir);
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (m_hotReloadState != HotReloadState::Idle) {
                LOG_INFO("[ScriptsCompiler] Hot reload already in progress, ignoring");
                return;
            }
            m_hotReloadState = HotReloadState::ScriptsChanged;
            m_pendingScriptsDir = scriptsDir;
        }
        m_stateChanged.notify_one();
    });

    auto deserializeCallback = m_scriptManager->GetDeserializeComponentFromJson();
    if (deserializeCallback) {
        RegisterComponentDeserializeCallback(deserializeCallback);
        LOG_INFO("[ScriptsCompiler] Registered component deserialize callback with editor");
    }
}

void ScriptsCompiler::Update() {
    if (!m_scriptManager) return;

    HotReloadState state;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        state = m_hotReloadState;
    }

    switch (state) {
        case HotReloadState::ScriptsChanged:
        {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_hotReloadState = HotReloadState::CompilingScripts;
            }

            LOG_INFO("[ScriptsCompiler] Starting hot reload: compiling scripts");

            if (m_hotReloadThread.joinable())
                m_hotReloadThread.join();

            m_hotReloadThread = std::thread(
                &ScriptsCompiler::_backgroundHotReloadOrchestrate, this);
            break;
        }

        case HotReloadState::UnloadingAssembly:
            _mainThreadUnload();
            break;

        case HotReloadState::LoadingAssembly:
            _mainThreadLoad();
            break;

        case HotReloadState::Failed:
        {
            LOG_ERROR("[ScriptsCompiler] Hot reload failed: " << m_lastError);
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_hotReloadState = HotReloadState::Idle;
            break;
        }

        default:
            break;
    }

    if (m_registryRebuildPending.exchange(false)) {
        ComponentRegistryUI::RebuildFromNativeRegistry();
        LOG_INFO("[ScriptsCompiler] Deferred registry rebuild completed");
    }
}


bool ScriptsCompiler::_doCompileScripts(const std::string& scriptsDir, const std::string& outputPath, std::string& diagnostics) {
    if (!m_scriptManager) return false;
    return m_scriptManager->CompileScriptsWithDiagnostics(scriptsDir, outputPath, diagnostics);
}

ECS::World* ScriptsCompiler::_getTargetWorld() {
    ECS::World* targetWorld = m_emptyWorld;
    auto* activeScene = m_engine->GetSceneManager().GetActive();
    if (activeScene) targetWorld = &activeScene->GetWorld();
    return targetWorld;
}

void ScriptsCompiler::_doLoadAndRegisterSystems(const std::string& assemblyPath) {
    if (!m_scriptManager->LoadAssembly(assemblyPath)) {
        LOG_ERROR("[ScriptsCompiler] Failed to load compiled script assembly");
        return;
    }

    LOG_INFO("[ScriptsCompiler] Loaded compiled script assembly");

    ECS::World* targetWorld = _getTargetWorld();

    // Clear C# components from entities before registering new systems
    auto clearComponentsFunc = m_scriptManager->GetClearAllManagedComponentsFunc();
    if (clearComponentsFunc) {
        clearComponentsFunc(targetWorld);
        LOG_INFO("[ScriptsCompiler] Cleared C# components from entities");
    }

    // Unregister old systems
    m_engine->GetSystemManager().UnregisterScriptedSystems(*targetWorld);
    LOG_INFO("[ScriptsCompiler] Unregistered old systems");

    // Register new systems
    int systemCount = m_scriptManager->RegisterScriptedSystems(m_engine->GetSystemManager(), targetWorld);
    LOG_INFO("[ScriptsCompiler] Registered " << systemCount << " C# systems");

    m_registryRebuildPending.store(true);
}

bool ScriptsCompiler::_doMoveCompiledAssemblyWithRetry(const std::string& tempPath, const std::string& finalPath, int maxRetries, int baseDelayMs) {
    try {
        std::filesystem::path tempDllPath = tempPath;
        std::filesystem::path tempPdbPath = std::filesystem::path(tempPath).replace_extension(".pdb");
        std::filesystem::path finalDllPath = finalPath;
        std::filesystem::path finalPdbPath = std::filesystem::path(finalPath).replace_extension(".pdb");
        
        if (!std::filesystem::exists(tempDllPath)) {
            LOG_WARNING("[ScriptsCompiler] Compiled DLL not found at temp location: " << tempDllPath.string());
            m_lastError = "Compiled DLL not found at temp location";
            return false;
        }
        
        // Remove old final DLL with retries (may be held by CoreCLR finalizers)
        int attempt = 0;
        while ((std::filesystem::exists(finalDllPath) || std::filesystem::exists(finalPdbPath)) && attempt < maxRetries) {
            try {
                if (std::filesystem::exists(finalDllPath)) {
                    std::filesystem::remove(finalDllPath);
                    LOG_INFO("[ScriptsCompiler] Removed old assembly file");
                }
                if (std::filesystem::exists(finalPdbPath)) {
                    std::filesystem::remove(finalPdbPath);
                    LOG_INFO("[ScriptsCompiler] Removed old PDB file");
                }
                break;
            }
            catch (const std::exception& e) {
                attempt++;
                if (attempt >= maxRetries) {
                    LOG_ERROR("[ScriptsCompiler] Failed to remove old assembly after " << maxRetries << " retries: " << e.what());
                    m_lastError = std::string("Failed to remove old assembly: ").append(e.what());
                    return false;
                }
                int delayMs = baseDelayMs * (1 << (attempt - 1));  // Exponential backoff
                LOG_INFO("[ScriptsCompiler] Retry " << attempt << "/" << maxRetries << " removing old assembly, waiting " << delayMs << "ms");
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        
        // Move temp to final with retries
        attempt = 0;
        while (attempt < maxRetries) {
            try {
                if (std::filesystem::exists(tempDllPath)) {
                    std::filesystem::rename(tempDllPath, finalDllPath);
                    LOG_INFO("[ScriptsCompiler] Moved compiled assembly: " << tempDllPath.string() << " -> " << finalDllPath.string());
                }
                if (std::filesystem::exists(tempPdbPath)) {
                    std::filesystem::rename(tempPdbPath, finalPdbPath);
                    LOG_INFO("[ScriptsCompiler] Moved PDB: " << tempPdbPath.string() << " -> " << finalPdbPath.string());
                }
                return true;
            }
            catch (const std::exception& e) {
                attempt++;
                if (attempt >= maxRetries) {
                    LOG_ERROR("[ScriptsCompiler] Failed to move assembly after " << maxRetries << " retries: " << e.what());
                    m_lastError = std::string("File move failed: ").append(e.what());
                    return false;
                }
                int delayMs = baseDelayMs * (1 << (attempt - 1));  // Exponential backoff
                LOG_INFO("[ScriptsCompiler] Retry " << attempt << "/" << maxRetries << " moving assembly, waiting " << delayMs << "ms");
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("[ScriptsCompiler] Unexpected error in assembly move: " << e.what());
        m_lastError = std::string("Unexpected error: ").append(e.what());
        return false;
    }
}

void ScriptsCompiler::_backgroundHotReloadOrchestrate() {
    if (!m_scriptManager) {
        LOG_ERROR("[ScriptsCompiler] ScriptManager is null, aborting hot reload");
        return;
    }

    try {
        std::string finalOutput = m_pendingScriptsOutput;
        std::replace(finalOutput.begin(), finalOutput.end(), '/', '\\');
        
        // Generate temp path for compilation (old assembly is locked during compilation)
        std::string tempOutput = finalOutput;
        {
            size_t dllPos = tempOutput.rfind(".dll");
            if (dllPos != std::string::npos) {
                tempOutput.insert(dllPos, "_temp");
            }
        }
        
        m_scriptManager->SetCompileStatus(1, 0, "Compiling...");
        
        // Step 1: Compile scripts to temp path (old assembly is locked, can't write to final)
        std::string diagnostics;
        bool compileSuccess = _doCompileScripts(m_pendingScriptsDir, tempOutput, diagnostics);
        
        if (!compileSuccess) {
            LOG_ERROR("[ScriptsCompiler] Hot reload: compilation failed - " << diagnostics);
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_hotReloadState = HotReloadState::Failed;
                m_lastError = diagnostics;
            }
            m_scriptManager->SetCompileStatus(4, 100, diagnostics.c_str());
            m_stateChanged.notify_one();
            return;
        }
        
        m_scriptManager->SetCompileStatus(2, 50, "Compiled successfully, unloading...");
        LOG_INFO("[ScriptsCompiler] Hot reload: compilation successful to temp: " << tempOutput);
        
        // Step 2: Signal main thread to unload assembly and move temp->final
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_pendingScriptsOutput = finalOutput;      // Final path for loading
            m_pendingTempScriptsOutput = tempOutput;   // Temp path to move from
            m_hotReloadState = HotReloadState::UnloadingAssembly;
        }
        m_stateChanged.notify_one();
        
        // Step 3: Wait for main thread to unload and move file, then transition to LoadingAssembly
        {
            std::unique_lock<std::mutex> lock(m_stateMutex);
            m_stateChanged.wait(lock, [this]() {
                return m_hotReloadState == HotReloadState::LoadingAssembly
                    || m_hotReloadState == HotReloadState::Failed
                    || m_shutdownRequested.load();
            });
            if (m_shutdownRequested.load()) return;
            if (m_hotReloadState == HotReloadState::Failed) return;
        }
        
        m_scriptManager->SetCompileStatus(2, 75, "Loading new assembly...");
        LOG_INFO("[ScriptsCompiler] Hot reload: main thread unloaded old assembly, waiting for load...");
    }
    catch (const std::exception& e) {
        LOG_ERROR("[ScriptsCompiler] Exception during hot reload: " << e.what());
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_hotReloadState = HotReloadState::Failed;
            m_lastError = std::string("Exception: ").append(e.what());
        }
        m_scriptManager->SetCompileStatus(4, 100, m_lastError.c_str());
        m_stateChanged.notify_one();
    }
}

void ScriptsCompiler::_mainThreadUnload() {
    LOG_INFO("[ScriptsCompiler] Unloading old assembly");

    // Set the current world so C# can clear components before unload
    ECS::World* targetWorld = _getTargetWorld();
    if (targetWorld) {
        m_scriptManager->SetCurrentWorldForHotReload(targetWorld);
        LOG_INFO("[ScriptsCompiler] Set current world for hot reload");
    }

    // Unload assembly:
    // This must be done on the main thread due to CoreCLR constraints
    // If unload fails, transition to Failed state
    // After unload, move the compiled assembly to final location
    // Then transition to LoadingAssembly state
    if (!m_scriptManager->UnloadScriptAssembly()) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_hotReloadState = HotReloadState::Failed;
        m_lastError = "Failed to unload old assembly";
        m_scriptManager->SetCompileStatus(4, 100, m_lastError.c_str());
        m_stateChanged.notify_one();
        return;
    }

    // IMPORTANT: Force CoreCLR to release file handles
    m_scriptManager->ForceGarbageCollection();

    // Give finalisers time to run (CoreCLR is async here)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    LOG_INFO("[ScriptsCompiler] Old assembly unload requested, GC triggered");
    
    // Try to move compiled assembly with retry + backoff
    if (!_doMoveCompiledAssemblyWithRetry(m_pendingTempScriptsOutput, m_pendingScriptsOutput)) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_hotReloadState = HotReloadState::Failed;
        }
        m_scriptManager->SetCompileStatus(4, 100, m_lastError.c_str());
        m_stateChanged.notify_one();
        return;
    }
    
    // Transition to LoadingAssembly for background thread
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_hotReloadState = HotReloadState::LoadingAssembly;
    }
    m_stateChanged.notify_one();
}

void ScriptsCompiler::_mainThreadLoad() {
    LOG_INFO("[ScriptsCompiler] Main thread: loading new assembly for hot reload");

    ECS::World* targetWorld = _getTargetWorld();

    // 1. Unregister old systems
    m_engine->GetSystemManager().UnregisterScriptedSystems(*targetWorld);
    LOG_INFO("[ScriptsCompiler] Unregistered old scripted systems");

    // 2. Load new assembly
    if (!m_scriptManager->LoadAssembly(m_pendingScriptsOutput)) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_hotReloadState = HotReloadState::Failed;
        m_lastError = "Failed to load assembly";
        m_scriptManager->SetCompileStatus(4, 100, m_lastError.c_str());
        m_stateChanged.notify_one();
        return;
    }

    LOG_INFO("[ScriptsCompiler] Loaded new script assembly");

    // 3. Register new systems
    int count = m_scriptManager->RegisterScriptedSystems(
        m_engine->GetSystemManager(), targetWorld);

    LOG_INFO("[ScriptsCompiler] Registered " << count << " new C# systems");

    m_registryRebuildPending.store(true);

    // 5. Final transition (main thread only)
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_hotReloadState = HotReloadState::Idle;
    }

    m_scriptManager->SetCompileStatus(3, 100, "Hot reload complete");
    m_stateChanged.notify_one();
}

void ScriptsCompiler::Shutdown() {
    m_shutdownRequested.store(true);
    m_stateChanged.notify_all();
    
    try {
        if (m_initialCompileThread.joinable()) m_initialCompileThread.join();
        if (m_hotReloadThread.joinable()) m_hotReloadThread.join();
    } catch (...) {}
    try {
        if (m_bgProgressThread.joinable()) m_bgProgressThread.join();
    } catch (...) {}
}
