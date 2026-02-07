/**
 * @Name: Dalton koh, 2403250
 * @email: d.koh@digipen.edu
 * @file    FadeSceneSwitch.cs
 * 
 * @brief   Manual scene switch with audio fade/crossfade.
 */

using System;
using System.IO;
using GrapeEngine.Scripting.Core;
// Input and key codes used for manual trigger.
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

// SystemBase scripts run globally each frame (not attached to a specific entity).
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class FadeSceneSwitch : SystemBase
{
    // Only run the switch logic when this is the active scene file.
    private const string SourceScenePath = "EchoesBelow/Scenes/MainScene.scn";
    // Scene file path relative to project root.
    private const string TargetScenePath = "EchoesBelow/Scenes/testscene2.scn";
    // Audio fade duration (seconds).
    private const float FadeSeconds = 2.0f;
    // Allow overlap between old and new audio during transition.
    private const bool AllowCrossfade = true;

    // Prevents repeated transitions once a switch succeeds or fails.
    private bool _switched;
    private bool _loggedInactiveScene;

    protected override void OnUpdate()
    {
        // One-shot behavior: stop if we already triggered the switch.
        if (_switched)
        {
            return;
        }

        // Make sure we're in the intended source scene file.
        SceneManager sceneManager = SceneManager.Instance;
        Scene? active = sceneManager.GetActive();
        if (active == null || !ScenePathMatches(active.Path, SourceScenePath))
        {
            return;
        }

        // Manual trigger: press G to start the transition.
        if (!Input.IsKeyPressed(KeyCode.G))
        {
            return;
        }

        // Request audio fade/crossfade for the next scene switch.
        sceneManager.SetNextAudioTransition(FadeSeconds, AllowCrossfade);
        //Log("FadeSceneSwitch: Beginning scene switch with audio transition.");
        // Load the target scene into the scene manager.
        Scene? loaded = sceneManager.LoadScene(TargetScenePath);
        if (loaded == null)
        {
            //Log($"FadeSceneSwitch: Failed to load target scene '{TargetScenePath}'.");
            _switched = true;
            return;
        }

        // Resolve the loaded scene's index so it can become active.
        ulong targetIndex = FindSceneIndex(sceneManager, TargetScenePath);
        if (targetIndex == ulong.MaxValue)
        {
            //Log($"FadeSceneSwitch: Could not resolve target scene index for '{TargetScenePath}'.");
            _switched = true;
            return;
        }

        // Activate the scene (switch occurs after fade logic in SceneManager).
        sceneManager.SetActive(targetIndex);
        _switched = true;
        //Log("FadeSceneSwitch: Switched to target scene after delay.");
    }

    // Find the index of a scene by matching its stored path.
    private static ulong FindSceneIndex(SceneManager sceneManager, string targetPath)
    {
        // Normalize path separators to improve matching reliability.
        string targetNormalized = NormalizePath(targetPath);
        ulong count = sceneManager.GetSceneCount();
        for (ulong i = 0; i < count; ++i)
        {
            Scene? scene = sceneManager.GetScene(i);
            if (scene == null)
            {
                continue;
            }

            // Compare normalized absolute/relative strings and suffixes.
            string scenePath = NormalizePath(scene.Path);
            if (ScenePathMatches(scenePath, targetNormalized))
            {
                return i;
            }
        }

        // Not found.
        return ulong.MaxValue;
    }

    // Compare normalized paths (case-insensitive) by exact match or suffix.
    private static bool ScenePathMatches(string scenePath, string targetPath)
    {
        string sceneNormalized = NormalizePath(scenePath);
        string targetNormalized = NormalizePath(targetPath);
        return string.Equals(sceneNormalized, targetNormalized, StringComparison.OrdinalIgnoreCase) ||
               sceneNormalized.EndsWith(targetNormalized, StringComparison.OrdinalIgnoreCase);
    }

    // Normalize path to use forward slashes and trim trailing separators.
    private static string NormalizePath(string path)
    {
        string normalized = path.Replace('\\', '/');
        return Path.TrimEndingDirectorySeparator(normalized);
    }
}
