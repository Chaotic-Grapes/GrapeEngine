using System;
using System.IO;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class AutoSceneSwitch : SystemBase
{
    private const string SourceSceneName = "AudioCrossfadeDemo";
    private const string TargetScenePath = "EchoesBelow/Scenes/Samples Demo/AudioCrossfadeTarget.scn";
    private const float SwitchDelaySeconds = 5.0f;

    private float _elapsed;
    private bool _switched;

    protected override void OnUpdate()
    {
        return;
        if (_switched)
        {
            return;
        }

        SceneManager sceneManager = SceneManager.Instance;
        Scene? active = sceneManager.GetActive();
        if (active == null || !string.Equals(active.Name, SourceSceneName, StringComparison.Ordinal))
        {
            Log($"AutoSceneSwitch: Active scene is not the source scene: '{active?.Name ?? "null"}'; aborting switch.");
            return;
        }

        _elapsed += Time.DeltaTime;
        if (_elapsed < SwitchDelaySeconds)
        {
            return;
        }

        sceneManager.SetNextAudioTransition(2.0f, true);
        Log("Beginning scene switch with audio transition.");
        Scene? loaded = sceneManager.LoadScene(TargetScenePath);
        Log("Switching");
        if (loaded == null)
        {
            Log($"AutoSceneSwitch: Failed to load target scene '{TargetScenePath}'.");
            _switched = true;
            return;
        }

        ulong targetIndex = FindSceneIndex(sceneManager, TargetScenePath);
        if (targetIndex == ulong.MaxValue)
        {
            Log($"AutoSceneSwitch: Could not resolve target scene index for '{TargetScenePath}'.");
            _switched = true;
            return;
        }

        sceneManager.SetActive(targetIndex);
        _switched = true;
        Log("AutoSceneSwitch: Switched to target scene after delay.");
    }

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

    private static string NormalizePath(string path)
    {
        string normalized = path.Replace('\\', '/');
        return Path.TrimEndingDirectorySeparator(normalized);
    }
}
