/* Start Header *****************************************************************/
/*!
\file   SceneManager.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for SceneManager. Provides scene management functionality from C#.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Defines how audio should behave during scene transitions.
/// </summary>
public enum SceneTransitionMode
{
    /// <summary>Instant switch, stop all audio immediately</summary>
    Immediate = 0,
    /// <summary>Fade out current scene audio, then switch</summary>
    FadeOut = 1,
    /// <summary>Overlap: fade out old scene while fading in new scene</summary>
    CrossFade = 2
}

/// <summary>
/// Manages scenes in the application. Handles scene loading, unloading, and activation.
/// </summary>
public class SceneManager
{
    private static SceneManager? _instance;
    private unsafe void* _sceneManagerPtr;
    private readonly Dictionary<ulong, Scene> _sceneCache = [];

    /// <summary>
    /// Gets the global SceneManager instance (singleton).
    /// </summary>
    public static SceneManager Instance
    {
        get
        {
            _instance ??= new SceneManager();
            return _instance;
        }
    }

    /// <summary>
    /// Internal constructor. Use Instance property to access the singleton.
    /// </summary>
    private SceneManager()
    {
        unsafe
        {
            _sceneManagerPtr = SceneAPI.GetSceneManagerInstance();
            if (_sceneManagerPtr == null)
            {
                throw new InvalidOperationException("Failed to get SceneManager instance");
            }
        }
    }

    /// <summary>
    /// Creates and adds a new empty scene to the manager.
    /// </summary>
    /// <returns>The index of the newly added scene.</returns>
    public ulong AddScene()
    {
        unsafe
        {
            return SceneAPI.AddScene(_sceneManagerPtr);
        }
    }

    /// <summary>
    /// Removes a scene from the manager by index.
    /// </summary>
    /// <param name="sceneIndex">The index of the scene to remove.</param>
    /// <returns>True if the scene was removed, false if the index was invalid.</returns>
    public bool RemoveScene(ulong sceneIndex)
    {
        unsafe
        {
            bool result = SceneAPI.RemoveScene(_sceneManagerPtr, sceneIndex);
            if (result)
            {
                _sceneCache.Remove(sceneIndex);
            }
            return result;
        }
    }

    /// <summary>
    /// Gets the total number of scenes managed.
    /// </summary>
    /// <returns>Number of scenes.</returns>
    public ulong GetSceneCount()
    {
        unsafe
        {
            return SceneAPI.GetSceneCount(_sceneManagerPtr);
        }
    }

    /// <summary>
    /// Gets a scene by index.
    /// </summary>
    /// <param name="sceneIndex">The index of the scene to retrieve.</param>
    /// <returns>The scene at the specified index, or null if the index is invalid.</returns>
    public Scene? GetScene(ulong sceneIndex)
    {
        if (_sceneCache.TryGetValue(sceneIndex, out var cached))
        {
            return cached;
        }

        unsafe
        {
            void* scenePtr = SceneAPI.GetScene(_sceneManagerPtr, sceneIndex);
            if (scenePtr == null)
            {
                return null;
            }

            var scene = new Scene(scenePtr);
            _sceneCache[sceneIndex] = scene;
            return scene;
        }
    }

    /// <summary>
    /// Queues a scene to become active on the next update boundary.
    /// </summary>
    /// <param name="sceneIndex">The index of the scene to activate.</param>
    public void SetActive(ulong sceneIndex)
    {
        unsafe
        {
            SceneAPI.SetActive(_sceneManagerPtr, sceneIndex);
        }
    }

    /// <summary>
    /// Immediately sets the active scene, performing necessary transitions.
    /// Use with caution as it bypasses the pending mechanism.
    /// </summary>
    /// <param name="sceneIndex">The index of the scene to activate.</param>
    public void SetActiveImmediate(ulong sceneIndex)
    {
        unsafe
        {
            SceneAPI.SetActiveImmediate(_sceneManagerPtr, sceneIndex);
        }
    }

    /// <summary>
    /// Sets the active scene with audio fade transition support.
    /// This queues the transition for the next update boundary.
    /// </summary>
    /// <param name="sceneIndex">The index of the scene to activate.</param>
    /// <param name="mode">Transition mode (Immediate, FadeOut, or CrossFade).</param>
    /// <param name="fadeDuration">Duration of fade in seconds (used for FadeOut and CrossFade modes).</param>
    public void SetActiveWithTransition(ulong sceneIndex, SceneTransitionMode mode = SceneTransitionMode.Immediate, float fadeDuration = 1.0f)
    {
        unsafe
        {
            SceneAPI.SetActiveWithTransition(_sceneManagerPtr, sceneIndex, (int)mode, fadeDuration);
        }
    }

    /// <summary>
    /// Gets the currently active scene.
    /// </summary>
    /// <returns>The active scene, or null if none is active.</returns>
    public Scene? GetActive()
    {
        unsafe
        {
            void* scenePtr = SceneAPI.GetActive(_sceneManagerPtr);
            if (scenePtr == null)
            {
                return null;
            }

            // Try to find from cache by pointer comparison
            foreach (var kvp in _sceneCache)
            {
                unsafe
                {
                    if (kvp.Value.NativePtr == scenePtr)
                    {
                        return kvp.Value;
                    }
                }
            }

            // Not cached, create new wrapper
            return new Scene(scenePtr);
        }
    }

    /// <summary>
    /// Gets the index of the currently active scene.
    /// </summary>
    /// <returns>Index of the active scene, or ulong.MaxValue if none is active.</returns>
    public ulong GetActiveIndex()
    {
        unsafe
        {
            return SceneAPI.GetActiveIndex(_sceneManagerPtr);
        }
    }

    /// <summary>
    /// Gets the index of the pending active scene.
    /// </summary>
    /// <returns>Index of the pending active scene, or ulong.MaxValue if none is pending.</returns>
    public ulong GetPendingIndex()
    {
        unsafe
        {
            return SceneAPI.GetPendingIndex(_sceneManagerPtr);
        }
    }

    /// <summary>
    /// Updates the scene manager and processes any pending scene transitions.
    /// This should be called once per frame by the engine.
    /// </summary>
    public void Update()
    {
        unsafe
        {
            SceneAPI.Update(_sceneManagerPtr);
        }
    }

    /// <summary>
    /// Saves a scene to a JSON file.
    /// </summary>
    /// <param name="sceneIndex">The index of the scene to save.</param>
    /// <param name="filename">Path to the output file.</param>
    /// <param name="sceneName">Optional scene name for metadata (defaults to scene's current name).</param>
    /// <param name="version">Optional version string for metadata (defaults to "1.0").</param>
    /// <returns>True if save was successful, false otherwise.</returns>
    public bool SaveScene(ulong sceneIndex, string filename, string? sceneName = null, string? version = null)
    {
        if (string.IsNullOrEmpty(filename))
            throw new ArgumentNullException(nameof(filename));

        sceneName ??= GetScene(sceneIndex)?.Name ?? "Scene";
        version ??= "1.0";

        unsafe
        {
            return SceneAPI.SaveScene(_sceneManagerPtr, sceneIndex, filename, sceneName, version);
        }
    }

    /// <summary>
    /// Loads a scene from a JSON file into the specified scene slot.
    /// This will destroy all existing entities in the target scene before loading.
    /// </summary>
    /// <param name="sceneIndex">The index of the scene slot to load into.</param>
    /// <param name="filename">Path to the input file.</param>
    /// <returns>True if load was successful, false otherwise.</returns>
    public bool LoadScene(ulong sceneIndex, string filename)
    {
        if (string.IsNullOrEmpty(filename))
            throw new ArgumentNullException(nameof(filename));

        unsafe
        {
            bool result = SceneAPI.LoadScene(_sceneManagerPtr, sceneIndex, filename);
            if (result)
            {
                // Invalidate cache for this scene since it was reloaded
                _sceneCache.Remove(sceneIndex);
            }
            return result;
        }
    }

    /// <summary>
    /// Internal method to access the native scene manager pointer.
    /// </summary>
    internal unsafe void* NativePtr => _sceneManagerPtr;
}

