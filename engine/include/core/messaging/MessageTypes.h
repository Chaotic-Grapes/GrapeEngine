/* Start Header *****************************************************************/
/*!
\file   MessageTypes.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   3rd November 2025
\brief
Defines message/event types used across the engine to enable decoupled
communication between systems (window, input, gameplay, audio, debug, etc.).
*/
/* End Header *******************************************************************/

#ifndef MESSAGETYPES_H
#define MESSAGETYPES_H

#include <string>
#include <functional>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include "math/Vector3D.h"
#include "math/Quaternion.h"

namespace Messaging {
    // -------------------------------
    // Window Events
    // -------------------------------

    // Sent when the window is resized.
    struct WindowResized {
        int Width;
        int Height;
        float AspectRatio;
        
        // Construct with new width/height; computes aspect ratio.
        WindowResized(int w, int h)
            : Width(w), Height(h), AspectRatio(static_cast<float>(w) / h) {
        }
    };

    // Sent when the window is requested to close.
    struct WindowClosed {
        bool UserInitiated; // True if the user closed the window
    };

    // Sent when window focus changes.
    struct WindowFocusChanged {
        bool HasFocus;
    };

    // -------------------------------
    // Input Events
    // -------------------------------

    // Sent when a key is pressed.
    struct KeyPressed {
        int Key;
        bool Repeat;
        int Modifiers; // Ctrl, Shift, Alt flags

        KeyPressed(int k, bool rep = false, int mods = 0)
            : Key(k), Repeat(rep), Modifiers(mods) {
        }
    };

    // Sent when a key is released.
    struct KeyReleased {
        int Key;
        int Modifiers; // Modifier flags 

        KeyReleased(int k, int mods = 0)
            : Key(k), Modifiers(mods) {
        }
    };

    // Sent when the mouse moves.
    struct MouseMoved {
        float X;        // Current x position
        float Y;        // Current y position
        float DeltaX; // Change in x since last event
        float DeltaY; // Change in y since last event
    };

    // Sent when a mouse button is pressed.
    struct MouseButtonPressed {
        int Button;
        float X; // Mouse x position 
        float Y; // Mouse y position
    };

    // Sent when a mouse button is released.
    struct MouseButtonReleased {
        int Button;
        float X; // Mouse x position 
        float Y; // Mouse y position
    };

    // Sent when mouse wheel is scrolled.
    struct MouseScrolled {
        float XOffset; // Horizontal scroll amount
        float YOffset; // Vertical scroll amount
    };

    // -------------------------------
    // Game/Entity Events
    // -------------------------------

    // Sent when an entity is created.
    struct EntityCreated {
        unsigned int EntityId; // ID of the new entity
        std::string EntityType; // Optional type name
    };

    // Sent when an entity is destroyed.
    struct EntityDestroyed {
        unsigned int EntityId; // ID of destroyed entity
    };

    // Sent when a collision between two entities occurs.
    struct CollisionDetected {
        unsigned int EntityA; // First entity involved
        unsigned int EntityB; // Second entity involved
        float ImpactForce; // Magnitude of impact
    };

    // Sent when an entity's health changes.
    struct HealthChanged {
        unsigned int EntityId; // ID of affected entity
        float OldHealth; // Health before change
        float NewHealth; // Health after change
        float MaxHealth; // Maximum health
    };

    // -------------------------------
    // System Events
    // -------------------------------

    // Sent when the active scene changes.
    struct SceneChanged {
        std::string OldScene; // Previous scene name
        std::string NewScene; // New scene name
    };

    // Sent when the game is paused/unpaused.
    struct GamePaused {
        bool IsPaused; // True if the game is paused
    };

    // Sent when a resource is loaded.
    struct ResourceLoaded {
        std::string ResourcePath; // Path of loaded resource
        std::string ResourceType; // Type of resource
        bool Success; // True if load succeeded
    };

    // Generic audio control event.
    struct AudioEvent {
        enum class Type { Play, Stop, Pause, Resume };
        Type EventType; //Action to perform
        std::string SoundId; // Sound resource ID
        float Volume; // Playback volume
        bool Loop; // Loop playback if true
    };

    // -------------------------------
    // Debug/Performance Events
    // -------------------------------


    // Performance warning issued by a system.
    struct PerformanceWarning {
        std::string System; // System reporting warning
        float DeltaTime; // Time elapsed this frame
        std::string Message; // Warning message
    };

    // Debug message for logging.
    struct DebugMessage {
        enum class Level { Info, Warning, Error };
        Level LogLevel; // Severity of message
        std::string Message; //  Message content
        std::string Source; // Originating system/module
    };

    // ========================================
    // Additional/Gameplay Events 
    // ========================================

    // Gameplay Events
    // Example gameplay event with three integer values.
    struct GamePlayHappenEvent {
        int ValueA;
        int ValueB;
        int ValueC;

        GamePlayHappenEvent(int a, int b, int c)
            : ValueA(a), ValueB(b), ValueC(c) {
        }
    };

    // Audio Control Events
    // Play a sound by name with volume.
    struct PlaySoundEvent {
        std::string SoundName;
        float Volume;

        explicit PlaySoundEvent(const std::string& name, float vol = 0.5f)
            : SoundName(name), Volume(vol) {
        }
    };

    // Pause a sound by name.
    struct PauseSoundEvent {
        std::string SoundName;

        explicit PauseSoundEvent(const std::string& name)
            : SoundName(name) {
        }
    };

    // Stop a sound by name.
    struct StopSoundEvent {
        std::string SoundName;

        explicit StopSoundEvent(const std::string& name)
            : SoundName(name) {
        }
    };

    // Change playback speed for a sound.
    struct SetSoundSpeedEvent {
        std::string SoundName;
        float Speed;

        SetSoundSpeedEvent(const std::string& name, float speed)
            : SoundName(name), Speed(speed) {
        }
    };

    // Change volume for a sound.
    struct SetSoundVolumeEvent {
        std::string SoundName;
        float Volume;

        SetSoundVolumeEvent(const std::string& name, float vol)
            : SoundName(name), Volume(vol) {
        }
    };

    /*struct WindowResized {
        int Width;
        int Height;
    };

    struct KeyPressed {
        int Key;
    };

    struct KeyReleased {
        int Key;
    };
    */

    // File path dropped onto the application/window.
    struct FileDropped {
        std::string filePath;
    };

    // Sent when the editor viewport panel is resized (for camera aspect ratio updates)
    struct ViewportResized {
        float Width;
        float Height;
        float AspectRatio;
        
        ViewportResized(float w, float h)
            : Width(w), Height(h), AspectRatio(w / h) {
        }
    };

    // -------------------------------
    // Editor Integration Events
    // -------------------------------

    // Sent when an entity's transform is modified (for undo system)
    struct EntityTransformChanged {
        uint32_t EntityId;
        Vector3D OldPosition;
        Quaternion OldRotation;
        Vector3D OldScale;
        Vector3D NewPosition;
        Quaternion NewRotation;
        Vector3D NewScale;

        EntityTransformChanged(uint32_t id, 
                             const Vector3D& oldPos, const Quaternion& oldRot, const Vector3D& oldScale,
                             const Vector3D& newPos, const Quaternion& newRot, const Vector3D& newScale)
            : EntityId(id), 
              OldPosition(oldPos), OldRotation(oldRot), OldScale(oldScale),
              NewPosition(newPos), NewRotation(newRot), NewScale(newScale) {
        }
    };

    // Sent when scene data is modified (for marking scene dirty)
    struct SceneModified {
        std::string Reason; // Optional description of what changed
        
        SceneModified(const std::string& reason = "")
            : Reason(reason) {
        }
    };

    // -------------------------------
    // Engine to Editor Communication Events
    // (Engine sends these, Editor subscribes)
    // -------------------------------

    // Request to select an entity in the editor hierarchy
    struct EntitySelectionRequested {
        uint32_t EntityId;
        bool AddToSelection; // True for multi-select
        
        EntitySelectionRequested(uint32_t id, bool add = false)
            : EntityId(id), AddToSelection(add) {
        }
    };

    // Request to focus the editor camera on an entity
    struct EntityFocusRequested {
        uint32_t EntityId;
        bool Animate; // True to animate the camera movement
        
        EntityFocusRequested(uint32_t id, bool animate = true)
            : EntityId(id), Animate(animate) {
        }
    };

    // Request to open an asset in the editor
    struct AssetOpenRequested {
        std::string AssetPath;
        std::string AssetType; // "Scene", "Prefab", "Script", etc.
        
        AssetOpenRequested(const std::string& path, const std::string& type)
            : AssetPath(path), AssetType(type) {
        }
    };

    // Notification that the engine wants to display debug information
    struct DebugVisualizationRequested {
        enum class Type {
            Physics,      // Physics colliders and forces
            Audio,        // Audio source positions and ranges
            Navmesh,      // Navigation mesh
            Profiling,    // Performance profiling data
            Custom        // Custom debug data
        };
        
        Type VisualizationType;
        bool Enabled;
        void* Data; // Pointer to debug data structure (type-specific)
        
        DebugVisualizationRequested(Type type, bool enabled, void* data = nullptr)
            : VisualizationType(type), Enabled(enabled), Data(data) {
        }
    };

    // Engine requests the editor to show a notification/message
    struct EditorNotificationRequested {
        enum class Severity {
            Info,
            Warning,
            Error,
            Success
        };
        
        Severity Level;
        std::string Title;
        std::string Message;
        float Duration; // Seconds to display (0 = persistent)
        
        EditorNotificationRequested(Severity level, const std::string& title, 
                                   const std::string& msg, float duration = 3.0f)
            : Level(level), Title(title), Message(msg), Duration(duration) {
        }
    };

    // Request to highlight entities in the viewport (for debugging)
    struct EntityHighlightRequested {
        std::vector<uint32_t> EntityIds;
        glm::vec4 Color;
        float Duration; // Seconds (0 = until cleared)
        
        EntityHighlightRequested(const std::vector<uint32_t>& ids, 
                                const glm::vec4& color, float duration = 0.0f)
            : EntityIds(ids), Color(color), Duration(duration) {
        }
    };

    // Request to update the inspector panel for an entity
    struct InspectorRefreshRequested {
        uint32_t EntityId; // 0 = refresh current selection
        
        InspectorRefreshRequested(uint32_t id = 0)
            : EntityId(id) {
        }
    };

    // Notification that a render pass completed (for editor viewport updates)
    struct RenderPassCompleted {
        enum class PassType {
            Scene,
            PostProcess,
            UI,
            Debug
        };
        
        PassType Pass;
        void* FramebufferHandle; // Platform-specific framebuffer handle
        
        RenderPassCompleted(PassType pass, void* fbHandle = nullptr)
            : Pass(pass), FramebufferHandle(fbHandle) {
        }
    };

    // Request editor to perform an action
    struct EditorCommandRequested {
        enum class Command {
            Save,
            SaveAs,
            Load,
            Undo,
            Redo,
            Cut,
            Copy,
            Paste,
            Delete,
            Duplicate,
            PlayScene,
            StopScene,
            PauseScene
        };
        
        Command CommandType;
        std::string Parameter; // Optional parameter for the command
        
        EditorCommandRequested(Command cmd, const std::string& param = "")
            : CommandType(cmd), Parameter(param) {
        }
    };

    // Notify that a system has finished initialization (for editor status display)
    struct SystemInitialized {
        std::string SystemName;
        bool Success;
        std::string ErrorMessage; // Empty if successful
        
        SystemInitialized(const std::string& name, bool success, 
                         const std::string& error = "")
            : SystemName(name), Success(success), ErrorMessage(error) {
        }
    };

    // Request for picking/raycast results (for editor selection)
    struct PickResultRequested {
        float ScreenX;
        float ScreenY;
        uint32_t ResultEntityId; // Filled by engine, read by editor
        bool HitSomething;
        glm::vec3 HitPosition;
        
        PickResultRequested(float x, float y)
            : ScreenX(x), ScreenY(y), ResultEntityId(0), 
              HitSomething(false), HitPosition(0.0f) {
        }
    };
}

#endif // MESSAGETYPES_H
