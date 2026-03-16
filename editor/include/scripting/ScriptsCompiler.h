/* Start Header *****************************************************************/
/*!
\file   ScriptsCompiler.h
\author Muhammad Nur Fadzly Bin Zulkifli
\date   26th January 2026
\brief
Declaration of the ScriptsCompiler class which manages C# script compilation
and hot-reload orchestration for the editor.

This header declares the public API used by the editor to start compilation,
poll per-frame updates, and shut down background work.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace Engine { class Application; }
namespace ECS { class World; class ScriptManager; }

enum class HotReloadState {
    Idle,
    ScriptsChanged,
    CompilingScripts,
    UnloadingAssembly,
    LoadingAssembly,
    Failed
};

/**
 * @brief Manages compilation and hot-reload orchestration for C# scripts.
 *
 * Responsibilities:
 * - Initial compilation of project scripts on startup.
 * - Starting the file watcher for incremental compilation.
 * - Orchestrating hot-reload (compile -> unload -> move -> load -> register).
 * - Owning the hot-reload state machine and CoreCLR-safe orchestration.
 */
class ScriptsCompiler {
public:
    /**
     * @brief Construct a ScriptsCompiler attached to the engine instance.
     * @param engine Pointer to the `Engine::Application` instance.
     * @param emptyWorld World used as fallback for system registration.
     */
    ScriptsCompiler(Engine::Application* engine, ECS::World* emptyWorld);
    ~ScriptsCompiler();

    /**
     * @brief Initialize the compiler with an existing `ScriptManager` instance.
     * @param scriptManager Pointer to `ECS::ScriptManager` (may be nullptr).
     */
    void Initialize(ECS::ScriptManager* scriptManager);

    /**
     * @brief Start initial compilation and begin watching scripts if possible.
     *
     * This will spawn background threads to compile and poll progress.
     */
    void Start();

    /**
     * @brief Per-frame update; drives hot-reload state machine and deferred work.
     *
     * Call this from the editor main loop each frame.
     */
    void Update();

    /**
     * @brief Shutdown background threads and clean up.
     */
    void Shutdown();

private:
    Engine::Application* m_engine{nullptr};
    ECS::World* m_emptyWorld{nullptr};
    ECS::ScriptManager* m_scriptManager{nullptr};

    // State machine with single-writer enforcement:
    // - File watcher sets: ScriptsChanged
    // - Background thread sets: CompilingScripts, Failed
    // - Main thread sets: UnloadingAssembly, LoadingAssembly, Idle
    HotReloadState m_hotReloadState{HotReloadState::Idle};
    std::string m_pendingScriptsDir;
    std::string m_pendingScriptsOutput;      // Final assembly path (destination after move)
    std::string m_pendingTempScriptsOutput;  // Temp assembly path (source for move)
    std::string m_lastError;
    
    std::mutex m_stateMutex;
    std::condition_variable m_stateChanged;
    std::atomic<bool> m_shutdownRequested{false};

    std::thread m_initialCompileThread;
    std::thread m_hotReloadThread;
    std::thread m_bgProgressThread;
    std::atomic<bool> m_compileDone{false};
    std::atomic<bool> m_registryRebuildPending{false};
    bool m_queuedScriptChange{false};
    std::string m_queuedScriptsDir;

    /** Register callbacks with the ScriptManager (hot-reload, file watcher). */
    void _registerCallbacks();

    /**
     * @brief Compile scripts into the given output assembly.
     * @param scriptsDir Root directory containing C# scripts to compile.
     * @param outputPath Path to write compiled assembly to.
     * @param diagnostics Out parameter to receive diagnostic text on failure.
     * @return true on success, false on failure.
     */
    bool _doCompileScripts(const std::string& scriptsDir, const std::string& outputPath, std::string& diagnostics);
    
    // Utility helpers
    /**
     * @brief Return the active scene world or the fallback empty world.
     */
    ECS::World* _getTargetWorld();

    /**
     * @brief Load the provided assembly and (re)register scripted systems.
     * @param assemblyPath Path to the compiled assembly to load.
     */
    void _doLoadAndRegisterSystems(const std::string& assemblyPath);

    // Hot-reload background orchestration (runs on background thread)
    void _backgroundHotReloadOrchestrate();
    
    // Main-thread hot-reload phase handlers
    void _mainThreadUnload();
    void _mainThreadLoad();
    
    // Helper to move compiled assembly with retry + backoff
    bool _doMoveCompiledAssemblyWithRetry(const std::string& tempPath, const std::string& finalPath, int maxRetries = 10, int baseDelayMs = 10);
};
