using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace Scripts;

[System(SystemGroup.Update, SystemRunMode.Always)]
[RequireForUpdate<GUIText>]
public class FPSSystem : SystemBase
{
    private ushort numFrames = 0;
    private const string FpsDisplayName = "FPS Display";
    private const ushort FramesToUpdate = 60;

    protected override void OnUpdate()
    {
        numFrames++;
        var input = Input.IsKeyPressed(KeyCode.Semicolon);

        foreach (var gameObject in World!.Query<GUIText, Active, Name>())
        {
            // var entity = gameObject.Entity;
            ref var guiText = ref gameObject.Component1;
            ref var active = ref gameObject.Component2;
            var name = Strings.Resolve(gameObject.Component3.Value);

            if (name != FpsDisplayName)
                continue;

            if (name == FpsDisplayName && input)
            {
                active.Enabled = !active.Enabled;
            }

            if (name == FpsDisplayName && active.Enabled && numFrames >= FramesToUpdate)
            {
                var fps = 1.0f / Time.DeltaTime;
                guiText.TextId = Strings.Intern($"FPS: {fps:F0}");
            }
            else if (name == FpsDisplayName && !active.Enabled)
            {
                guiText.TextId = Strings.Intern(string.Empty);
            }
        }
        numFrames %= FramesToUpdate;
    }
}
