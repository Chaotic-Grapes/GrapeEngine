using System;
using System.IO;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
// Auto switch from one scene to another after a delay.
public class AutoSceneSwitch : SystemBase
{
    // Source scene to watch.
    private const string SourceSceneName = "AudioCrossfadeDemo";
    // Target scene path to load/activate.
    private const string TargetScenePath = "EchoesBelow/Scenes/Samples Demo/AudioCrossfadeTarget.scn";
    // Delay before switching.
    private const float SwitchDelaySeconds = 5.0f;

    // Runtime state.
    private float _elapsed;
    private bool _switched;
    private bool _loggedInactiveScene;

    protected override void OnUpdate()
    {
        // Only switch once.
        if (_switched)
        {
            return;
        }

        // Only act in the source scene.
        SceneManager sceneManager = SceneManager.Instance;
        Scene? active = sceneManager.GetActive();
        if (active == null || !string.Equals(active.Name, SourceSceneName, StringComparison.Ordinal))
        {
            return;
        }

        // Wait for the delay.
        _elapsed += Time.DeltaTime;
        if (_elapsed < SwitchDelaySeconds)
        {
            return;
        }

        // Start audio transition and load target.
        sceneManager.SetNextAudioTransition(2.0f, true);
        //Log("Beginning scene switch with audio transition.");
        Scene? loaded = sceneManager.LoadScene(TargetScenePath);
        //Log("Switching");
        if (loaded == null)
        {
            //Log($"AutoSceneSwitch: Failed to load target scene '{TargetScenePath}'.");
            _switched = true;
            return;
        }

        // Find the loaded scene index.
        ulong targetIndex = FindSceneIndex(sceneManager, TargetScenePath);
        if (targetIndex == ulong.MaxValue)
        {
            //Log($"AutoSceneSwitch: Could not resolve target scene index for '{TargetScenePath}'.");
            _switched = true;
            return;
        }

        // Activate target scene.
        sceneManager.SetActive(targetIndex);
        _switched = true;
        //Log("AutoSceneSwitch: Switched to target scene after delay.");
    }

    // Find scene index by path.
    private static ulong FindSceneIndex(SceneManager sceneManager, string targetPath)
    {
        string targetNormalized = NormalizePath(targetPath);
        ulong count = sceneManager.GetSceneCount();
        for (ulong i = 0; i < count; ++i)
        {
            Scene? scene = sceneManager.GetScene(i);
            if (scene == null)
            {
                continue;
            }

            string scenePath = NormalizePath(scene.Path);
            if (string.Equals(scenePath, targetNormalized, StringComparison.OrdinalIgnoreCase) ||
                scenePath.EndsWith(targetNormalized, StringComparison.OrdinalIgnoreCase))
            {
                return i;
            }
        }

        return ulong.MaxValue;
    }

    // Normalize path for comparison.
    private static string NormalizePath(string path)
    {
        string normalized = path.Replace('\\', '/');
        return Path.TrimEndingDirectorySeparator(normalized);
    }
}
