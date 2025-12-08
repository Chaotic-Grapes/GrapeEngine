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

namespace GrapeEngine.Scripting.Attributes;

/// <summary>
/// Marks a system class with the execution group (phase) it should run in.
/// If not specified, defaults to SystemGroup.Update.
/// </summary>
[AttributeUsage(AttributeTargets.Class)]
public class SystemGroupAttribute(SystemGroup group) : Attribute
{
    /// <summary>
    /// The execution group this system runs in.
    /// </summary>
    public SystemGroup Group { get; } = group;
}
