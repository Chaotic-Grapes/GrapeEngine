/*****************************************************************************/
/*!
\file       MessageTypes.h
\author     Samantha Leong (s.leong@digipen.edu)
\par        DigiPen login: 2403088
\date       2025-11-03
\brief
 * Defines a collection of message/event types used across the engine to allow
 * decoupled communication between systems. Each struct represents a specific
 * type of event (window, input, gameplay, audio, debug, etc.) that can be
 * broadcast via the MessageBus system.
 *
 * @Usage:
 * Each system can subscribe to a specific message type and receive it when
 * broadcast:
 * @code{.cpp}
 * // Example: subscribing to a collision event
 * auto handle = Messaging::MessageSystem::Subscribe<Messaging::CollisionDetected>(
 *     [](const Messaging::CollisionDetected& e) {
 *         std::cout << "Collision between " << e.EntityA << " and " << e.EntityB
 *                   << " with impact " << e.ImpactForce << std::endl;
 *     }
 * );
 *
 * // Broadcast example
 * Messaging::CollisionDetected collision{ playerID, wallID, 9.8f };
 * Messaging::MessageSystem::Notify(collision);
 * @endcode
 *
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without prior
written consent of DigiPen Institute of Technology is prohibited.
*/
/*****************************************************************************/

#ifndef MESSAGETYPES_H
#define MESSAGETYPES_H

#include <string>


namespace Messaging {
    // -------------------------------
    // Window Events
    // -------------------------------

    /**
    * @struct WindowResized
    * @brief Sent when the window is resized.
    */
    struct WindowResized {
        int Width;
        int Height;
        float AspectRatio;
        
        /**
         * @brief Constructs a WindowResized event
         * @param w New width
         * @param h New height
         */
        WindowResized(int w, int h)
            : Width(w), Height(h), AspectRatio(static_cast<float>(w) / h) {
        }
    };

    /**
     * @struct WindowClosed
     * @brief Sent when the window is requested to close.
     */
    struct WindowClosed {
        bool UserInitiated; // True if the user closed the window
    };

    /**
     * @struct WindowFocusChanged
     * @brief Sent when window focus changes.
     */
    struct WindowFocusChanged {
        bool HasFocus;
    };

    // -------------------------------
    // Input Events
    // -------------------------------

    /**
     * @struct KeyPressed
     * @brief Sent when a key is pressed.
     */
    struct KeyPressed {
        int Key;
        bool Repeat;
        int Modifiers; // Ctrl, Shift, Alt flags

        KeyPressed(int k, bool rep = false, int mods = 0)
            : Key(k), Repeat(rep), Modifiers(mods) {
        }
    };

    /**
     * @struct KeyReleased
     * @brief Sent when a key is released.
     */
    struct KeyReleased {
        int Key;
        int Modifiers; // Modifier flags 

        KeyReleased(int k, int mods = 0)
            : Key(k), Modifiers(mods) {
        }
    };

    /**
     * @struct MouseMoved
     * @brief Sent when the mouse moves.
     */
    struct MouseMoved {
        float X;        // Current x position
        float Y;        // Current y position
        float DeltaX; // Change in x since last event
        float DeltaY; // Change in y since last event
    };

    /**
     * @struct MouseButtonPressed
     * @brief Sent when a mouse button is pressed.
     */
    struct MouseButtonPressed {
        int Button;
        float X; // Mouse x position 
        float Y; // Mouse y position
    };

    /**
     * @struct MouseButtonReleased
     * @brief Sent when a mouse button is released.
     */
    struct MouseButtonReleased {
        int Button;
        float X; // Mouse x position 
        float Y; // Mouse y position
    };

    /**
     * @struct MouseScrolled
     * @brief Sent when mouse wheel is scrolled.
     */
    struct MouseScrolled {
        float XOffset; // Horizontal scroll amount
        float YOffset; // Vertical scroll amount
    };

    // -------------------------------
    // Game/Entity Events
    // -------------------------------

    /**
     * @struct EntityCreated
     * @brief Sent when an entity is created.
     */
    struct EntityCreated {
        unsigned int EntityId; // ID of the new entity
        std::string EntityType; // Optional type name
    };

    /**
     * @struct EntityDestroyed
     * @brief Sent when an entity is destroyed.
     */
    struct EntityDestroyed {
        unsigned int EntityId; // ID of destroyed entity
    };

    /**
     * @struct CollisionDetected
     * @brief Sent when a collision between two entities occurs.
     */
    struct CollisionDetected {
        unsigned int EntityA; // First entity involved
        unsigned int EntityB; // Second entity involved
        float ImpactForce; // Magnitude of impact
    };

    /**
     * @struct HealthChanged
     * @brief Sent when an entity's health changes.
     */
    struct HealthChanged {
        unsigned int EntityId; // ID of affected entity
        float OldHealth; // Health before change
        float NewHealth; // Health after change
        float MaxHealth; // Maximum health
    };

    // -------------------------------
    // System Events
    // -------------------------------

    struct SceneChanged {
        std::string OldScene; // Previous scene name
        std::string NewScene; // New scene name
    };

    struct GamePaused {
        bool IsPaused; // True if the game is paused
    };

    struct ResourceLoaded {
        std::string ResourcePath; // Path of loaded resource
        std::string ResourceType; // Type of resource
        bool Success; // True if load succeeded
    };

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


    struct PerformanceWarning {
        std::string System; // System reporting warning
        float DeltaTime; // Time elapsed this frame
        std::string Message; // Warning message
    };

    struct DebugMessage {
        enum class Level { Info, Warning, Error };
        Level LogLevel; // Severity of message
        std::string Message; //  Message content
        std::string Source; // Originating system/module
    };

    // ========================================
    // Additional/Gameplay Events 
    // ========================================

    // IMGUI Panel Management
    struct AddIMGUIPanelEvent {
        std::string Name; //Panel name
        std::function<void()> DrawFunction; // Function to draw panel
        std::function<void(int)> OnAddedCallback; // Optional callback after adding panel

        explicit AddIMGUIPanelEvent(
            std::string panelName,
            std::function<void()> drawFn,
            std::function<void(int)> cb = nullptr
        ) : Name(std::move(panelName)),
            DrawFunction(std::move(drawFn)),
            OnAddedCallback(std::move(cb)) {
        }
    };

    // Gameplay Events
    struct GamePlayHappenEvent {
        int ValueA;
        int ValueB;
        int ValueC;

        GamePlayHappenEvent(int a, int b, int c)
            : ValueA(a), ValueB(b), ValueC(c) {
        }
    };

    // Audio Control Events
    struct PlaySoundEvent {
        std::string SoundName;
        float Volume;

        explicit PlaySoundEvent(const std::string& name, float vol = 0.5f)
            : SoundName(name), Volume(vol) {
        }
    };

    struct PauseSoundEvent {
        std::string SoundName;

        explicit PauseSoundEvent(const std::string& name)
            : SoundName(name) {
        }
    };

    struct StopSoundEvent {
        std::string SoundName;

        explicit StopSoundEvent(const std::string& name)
            : SoundName(name) {
        }
    };

    struct SetSoundSpeedEvent {
        std::string SoundName;
        float Speed;

        SetSoundSpeedEvent(const std::string& name, float speed)
            : SoundName(name), Speed(speed) {
        }
    };

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
    };*/


}

#endif // MESSAGETYPES_H
