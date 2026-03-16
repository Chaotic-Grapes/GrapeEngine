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
/// SystemRunMode.PlayOnly.</param>
[AttributeUsage(AttributeTargets.Class, Inherited = true)]
public class SystemAttribute(SystemGroup group, SystemRunMode runMode = SystemRunMode.PlayOnly) : Attribute
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
