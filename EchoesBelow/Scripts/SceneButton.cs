using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;

namespace EchoesBelow.Scripts;

/// <summary>
/// Component that marks an entity as a scene transition button.
/// Add this to a button entity to make it trigger a scene change when clicked.
/// </summary>
[Component]
public record struct SceneButtonComponent(
    ulong TargetSceneIndex,         // Which scene to load (0, 1, 2, etc.)
    int TransitionMode,             // 0=Immediate, 1=FadeOut, 2=CrossFade
    float FadeDuration              // How long the fade takes in seconds
);

/// <summary>
/// System that handles scene button clicks.
/// Checks for mouse clicks and triggers scene transitions.
/// </summary>
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class SceneButtonSystem : SystemBase
{
    private bool wasAnyMousePressed = false;

    protected override void OnCreate()
    {
        Log("[SceneButtonSystem] Created", LogLevel.Info);
        System.Console.WriteLine("========================================");
        System.Console.WriteLine("[SceneButtonSystem] System created!");
        System.Console.WriteLine("========================================");
    }

    protected override void OnUpdate()
    {
        bool isAnyMousePressed =
            Input.IsMousePressed(MouseButton.Left) ||
            Input.IsMousePressed(MouseButton.Right) ||
            Input.IsMousePressed(MouseButton.Middle);

        bool isTransitionKeyPressed = Input.IsKeyPressed(KeyCode.G);

        // Detect mouse click (press -> down) or key press
        if ((!wasAnyMousePressed && isAnyMousePressed) || isTransitionKeyPressed)
        {
            System.Console.WriteLine("========================================");
            System.Console.WriteLine("[SceneButtonSystem] Transition input detected!");
            System.Console.WriteLine("========================================");

            // Count how many scene buttons exist
            int buttonCount = 0;
            foreach (var result in World!.Query<SceneButtonComponent>())
            {
                buttonCount++;
                var entity = result.Entity;
                ref var button = ref result.Component1;

                // For now, any click triggers the first scene button found
                Log($"[SceneButtonSystem] Button clicked! Transitioning to scene {button.TargetSceneIndex}", LogLevel.Info);
                System.Console.WriteLine($"[SceneButtonSystem] TRANSITIONING TO SCENE {button.TargetSceneIndex}");

                SceneTransitionMode mode = button.TransitionMode switch
                {
                    0 => SceneTransitionMode.Immediate,
                    1 => SceneTransitionMode.FadeOut,
                    2 => SceneTransitionMode.CrossFade,
                    3 => SceneTransitionMode.CrossFade,
                    _ => SceneTransitionMode.Immediate
                };
                string? scenePath = SceneManager.Instance.GetSceneListEntry(button.TargetSceneIndex);
                if (string.IsNullOrEmpty(scenePath))
                {
                    Log($"[SceneButtonSystem] Invalid scene index {button.TargetSceneIndex} - no SceneList entry found.", LogLevel.Warning);
                    break;
                }

                SceneManager.Instance.SetActiveWithTransition(scenePath, mode, button.FadeDuration);

                // Only trigger one button per click
                break;
            }

            if (buttonCount == 0)
            {
                System.Console.WriteLine("[SceneButtonSystem] WARNING: No entities with SceneButtonComponent found!");
            }
        }

        wasAnyMousePressed = isAnyMousePressed;
    }

    protected override void OnDestroy()
    {
        Log("[SceneButtonSystem] Destroyed", LogLevel.Debug);
    }
}
