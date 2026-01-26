/* Start Header *****************************************************************/
/*!
\file   SystemGroupAttribute.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Attribute for declaring which execution group a system should run in.
Allows systems to specify whether they run in PreUpdate, Update, PostUpdate, etc.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Systems.Attributes;

/// <summary>
/// Specifies that a class is a system and defines its execution group and run mode.
/// </summary>
/// <param name="group">The execution group in which the system will run. Determines the order and context in which the system is executed.</param>
/// <param name="runMode">The run mode for the system. Specifies whether the system runs in edit mode, play mode, or both. The default is
/// SystemRunMode.EditOnly.</param>
[AttributeUsage(AttributeTargets.Class)]
public class SystemAttribute(SystemGroup group, SystemRunMode runMode = SystemRunMode.EditOnly) : Attribute
{
    /// <summary>
    /// The execution group this system runs in.
    /// </summary>
    public SystemGroup Group { get; } = group;

    /// <summary>
    /// Gets the current run mode of the system.
    /// </summary>
    public SystemRunMode RunMode { get; } = runMode;
}

// ============================================================================
// SCRIPTING ENUMS - Must match C++ Engine definitions
// ============================================================================
// These enums are marshaled between C# and C++ and MUST have identical values.
// Validation: See EnumParity validator below
// 
// C++ Source: engine/core/ecs/SystemGroup.h
// C# Validation: ScriptHost.ValidateEnumParity()
// ============================================================================

/// <summary>
/// System execution group - defines when systems run relative to engine lifecycle.
/// 
/// MUST match C++ ECS::SystemGroup enum values for correct P/Invoke marshaling.
/// If C++ values change, update both enums AND the validation logic below.
/// </summary>
public enum SystemGroup
{
    /// <summary>
    /// Systems execute before main update cycle (frame setup, input processing)
    /// </summary>
    PreUpdate = 0,

    /// <summary>
    /// Main update systems (gameplay logic, AI, etc.)
    /// </summary>
    Update = 1,

    /// <summary>
    /// Systems execute after update (cleanup, state finalization)
    /// </summary>
    PostUpdate = 2,

    /// <summary>
    /// Physics pre-calculation phase
    /// </summary>
    PrePhysics = 3,

    /// <summary>
    /// Physics simulation systems
    /// </summary>
    Physics = 4,

    /// <summary>
    /// Physics post-calculation phase (resolution, callbacks)
    /// </summary>
    PostPhysics = 5,

    /// <summary>
    /// Rendering preparation phase
    /// </summary>
    PreRender = 6,

    /// <summary>
    /// Render systems (camera updates, draw calls)
    /// </summary>
    Render = 7,

    /// <summary>
    /// Post-render cleanup (frame finalization)
    /// </summary>
    PostRender = 8
}

/// <summary>
/// System execution mode - determines when systems are active.
/// 
/// MUST match C++ ECS::SystemRunMode enum values for correct P/Invoke marshaling.
/// If C++ values change, update both enums AND the validation logic below.
/// </summary>
public enum SystemRunMode
{
    /// <summary>
    /// System always runs (edit and play mode)
    /// </summary>
    Always = 0,

    /// <summary>
    /// System runs only in play mode
    /// </summary>
    PlayOnly = 1,

    /// <summary>
    /// System runs only in editor/edit mode
    /// </summary>
    EditOnly = 2
}
