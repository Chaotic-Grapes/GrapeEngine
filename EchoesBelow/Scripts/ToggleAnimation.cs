// Simple script to toggle between WALK and JUMP animations for the player entity.

using GrapeEngine.Scripting;
using System;

namespace Scripts;

public class ToggleAnimation : ScriptBehaviour
{
    // Animation layout assumptions (match the scene's SpriteSheetAnimation2D values):
    // - WALK: start 0, count 6, fps 12, loop=true
    // - JUMP: start 6, count 4, fps 10, loop=false

    private int walkStart = 0;
    private int walkCount = 6;
    private float walkFPS = 12f;
    private bool walkLoop = true;

    private int jumpStart = 6;
    private int jumpCount = 4;
    private float jumpFPS = 10f;
    private bool jumpLoop = false;
    private int cols = 1;
    private int totalFrames = 0;

    public override void OnStart()
    {
        // Read sprite-sheet layout from the attached component so we don't hardcode texture values here.
        try
        {
            ref var anim = ref GetComponent<SpriteSheetAnimation2D>();
            if (anim.FrameWidth > 0 && anim.SheetWidth > 0)
            {
                cols = Math.Max(1, anim.SheetWidth / anim.FrameWidth);
            }
            else
            {
                cols = 1;
            }

            int rows = 1;
            if (anim.FrameHeight > 0 && anim.SheetHeight > 0)
                rows = Math.Max(1, anim.SheetHeight / anim.FrameHeight);

            totalFrames = cols * rows;

            // Use existing component values as the default walk animation
            walkStart = anim.StartFrame;
            walkCount = anim.FrameCount > 0 ? anim.FrameCount : walkCount;

            // Place jump animation on the next row if available
            jumpStart = Math.Min(totalFrames - 1, walkStart + cols);
            // Keep jump count reasonable (use same as walk or remaining frames)
            jumpCount = Math.Min(walkCount, Math.Max(1, totalFrames - jumpStart));
        }
        catch (Exception)
        {
            cols = 1;
            totalFrames = walkCount;
        }

        // Start in walk animation
        PlayWalk();
    }

    public override void OnUpdate()
    {
        if (Input.IsKeyDown(KeyCode.Space))
        {
            PlayJump();
        }

        if (Input.IsKeyDown(KeyCode.R))
        {
            PlayWalk();
        }
    }

    private void PlayWalk()
    {
        try
        {
            ref var anim = ref GetComponent<SpriteSheetAnimation2D>();
            ref var state = ref GetComponent<AnimationState2D>();
            // Only modify the animation range and playback parameters
            anim.StartFrame = walkStart;
            anim.FrameCount = walkCount;
            anim.FramesPerSecond = walkFPS;
            anim.Loop = walkLoop;
            anim.Playing = true;

            state.CurrentFrame = 0;
            state.TimeAccumulator = 0.0f;
            state.Finished = false;

            Log($"ToggleAnimation: PlayWalk start={walkStart} count={walkCount} cols={cols}");
        }
        catch (Exception)
        {
            // If components are missing, do nothing — scene should attach both components
        }
    }

    private void PlayJump()
    {
        try
        {
            ref var anim = ref GetComponent<SpriteSheetAnimation2D>();
            ref var state = ref GetComponent<AnimationState2D>();
            // Only modify the animation range and playback parameters
            anim.StartFrame = jumpStart;
            anim.FrameCount = jumpCount;
            anim.FramesPerSecond = jumpFPS;
            anim.Loop = jumpLoop;
            anim.Playing = true;

            state.CurrentFrame = 0;
            state.TimeAccumulator = 0.0f;
            state.Finished = false;

            Log($"ToggleAnimation: PlayJump start={jumpStart} count={jumpCount} cols={cols}");
        }
        catch (Exception)
        {
            // If components are missing, do nothing
        }
    }
}
