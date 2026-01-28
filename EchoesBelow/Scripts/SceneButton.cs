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
    int TargetSceneIndex,           // Which scene to load (0, 1, 2, etc.)
    int TransitionMode,             // 0=Immediate, 1=FadeOut, 2=CrossFade
    float FadeDuration              // How long the fade takes in seconds
);

/// <summary>
/// System that handles scene button clicks.
/// Checks for mouse clicks and triggers scene transitions.
/// </summary>
[SystemGroup(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class SceneButtonSystem : SystemBase
{
    private bool wasMousePressed = false;

    protected override void OnCreate()
    {
        Log("[SceneButtonSystem] Created", LogLevel.Info);
        System.Console.WriteLine("========================================");
        System.Console.WriteLine("[SceneButtonSystem] System created!");
        System.Console.WriteLine("========================================");
    }

    protected override void OnUpdate()
    {
        bool isMousePressed = Input.IsMousePressed(MouseButton.Left);

        // Detect mouse click (press -> release)
        if (wasMousePressed && !isMousePressed)
        {
            System.Console.WriteLine("========================================");
            System.Console.WriteLine("[SceneButtonSystem] Mouse clicked!");
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

                SceneTransitionMode mode = (SceneTransitionMode)button.TransitionMode;
                SceneManager.SetActiveWithTransition(button.TargetSceneIndex, mode, button.FadeDuration);

                // Only trigger one button per click
                break;
            }

            if (buttonCount == 0)
            {
                System.Console.WriteLine("[SceneButtonSystem] WARNING: No entities with SceneButtonComponent found!");
            }
        }

        wasMousePressed = isMousePressed;
    }

    protected override void OnDestroy()
    {
        Log("[SceneButtonSystem] Destroyed", LogLevel.Debug);
    }
}