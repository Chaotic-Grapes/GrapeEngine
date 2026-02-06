using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;

namespace EchoesBelow.Scripts;

[System(SystemGroup.Update, SystemRunMode.Always)]
public class FPS : SystemBase
{
    protected override void OnUpdate()
    {
        var input = Input.IsKeyPressed(KeyCode.Semicolon);

        foreach (var gameObject in World!.Query<GUIText, Active, Name>())
        {
            // var entity = gameObject.Entity;
            ref var guiText = ref gameObject.Component1;
            ref var active = ref gameObject.Component2;
            var name = Strings.Resolve(gameObject.Component3.Value);

            if (name == "FPS Display" && input)
            {
                active.Enabled = !active.Enabled;
            }

            if (name == "FPS Display" && active.Enabled)
            {
                var fps = 1.0f / Time.DeltaTime;
                guiText.TextId = Strings.Intern($"FPS: {fps:F2}");
            }
            else if (name == "FPS Display" && !active.Enabled)
            {
                guiText.TextId = Strings.Intern(string.Empty);
            }
        }
    }
}