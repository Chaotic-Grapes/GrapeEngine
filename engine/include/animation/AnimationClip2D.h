/* Start Header *****************************************************************/
/*!
\file   AnimationClip2D.h
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Defines data structures for 2D animation clips loaded from .anim assets.
*/
/* End Header *******************************************************************/

#pragma once

#include <string>
#include <vector>
#include "Color.h"
#include "math/Vector2D.h"

namespace Animation {
    /**
     * @brief Represents a notify event that can be triggered at a specific frame in an animation clip.
     */
    struct AnimationFrameNotify2D {
        std::string Name; // The name of the notify event, which can be used to identify the type of event (e.g., "Footstep", "AttackHit", etc.)
    };

    /**
     * @brief Represents a hitbox associated with a specific frame in an animation clip, used for collision detection or damage calculation.
     */
    struct AnimationFrameHitbox2D {
        std::string Name;                       // The name of the hitbox, which can be used to identify its purpose (e.g., "Head", "Body", "Weapon", etc.)
        Vector2D Offset{0.0f, 0.0f};            // The offset of the hitbox relative to the frame's origin
        Vector2D Size{0.0f, 0.0f};              // The size of the hitbox
        Color Color{1.0f, 0.0f, 0.0f, 1.0f};    // The color of the hitbox for debugging purposes
    };

    /**
     * @brief Represents an attachment point associated with a specific frame in an animation clip, used for attaching other entities or effects.
     */
    struct AnimationFrameAttachment2D {
        std::string Name;                       // The name of the attachment point, which can be used to identify its purpose (e.g., "Hand", "Weapon", "Effect", etc.)
        Vector2D Offset{0.0f, 0.0f};            // The offset of the attachment point relative to the frame's origin
    };

    /**
    * @brief Represents the data for a single frame in an animation clip, including root motion, notifies, hitboxes, and attachments.
    */
    struct AnimationFrameData2D {
        Vector2D RootMotion{0.0f, 0.0f};                        // The root motion delta for this frame, which can be applied to the entity's position when this frame is active
        std::vector<AnimationFrameNotify2D> Notifies;           // A list of notify events that should be triggered when this frame is active
        std::vector<AnimationFrameHitbox2D> Hitboxes;           // A list of hitboxes that should be active when this frame is active, used for collision detection or damage calculation
        std::vector<AnimationFrameAttachment2D> Attachments;    // A list of attachment points that should be active when this frame is active, used for attaching other entities or effects    
    };

    /**
     * @brief Represents the data for a 2D animation clip, including its name, sprite sheet information, frame durations, and per-frame data.
     */
    struct AnimationClipSpriteSheet2D {
        std::string TexturePath;           // The file path to the texture used for the sprite sheet
        std::string NormalTexturePath;     // The file path to the normal map texture used for the sprite sheet
        int FrameWidth = 0;                // The width of a single frame in the sprite sheet
        int FrameHeight = 0;               // The height of a single frame in the sprite sheet
        int SheetWidth = 0;                // The width of the entire sprite sheet
        int SheetHeight = 0;               // The height of the entire sprite sheet
        int StartFrame = 0;                // The index of the first frame in the animation clip
        int FrameCount = 0;                // The total number of frames in the animation clip
        int Row = 0;                       // The row in the sprite sheet where the animation frames are located
        int FrameOffset = 0;               // The offset between frames in the sprite sheet
        int FrameLength = 0;               // The length of each frame in the animation clip
        float FramesPerSecond = 10.0f;     // The playback speed of the animation clip in frames per second
        bool Loop = true;                  // Whether the animation clip should loop
        bool UseRow = false;               // Whether to use the specified row for the animation frames
    };

    /**
     * @brief Represents the data for a 2D animation clip, including its name, sprite sheet information, frame durations, and per-frame data.
     */
    struct AnimationClip2DData {
        std::string Name;                           // The name of the animation clip, which can be used to identify it within an animation controller
        AnimationClipSpriteSheet2D SpriteSheet;     // The sprite sheet data for the animation clip
        std::vector<float> FrameDurations;          // The duration of each frame in the animation clip
        std::vector<AnimationFrameData2D> Frames;   // The per-frame data for the animation clip
    };
}
