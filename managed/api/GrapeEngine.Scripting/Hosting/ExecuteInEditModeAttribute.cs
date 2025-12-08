/* Start Header *****************************************************************/
/*!
\file   ExecuteInEditModeAttribute.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Attribute to mark systems that should run in editor edit mode.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// Mark an ISystem with this attribute to execute in editor edit mode.
/// By default, systems only run during play mode. Systems marked with this
/// attribute will also run when the editor is in edit mode.
/// 
/// Useful for procedural generation, preview systems, and debug visualization.
/// </summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
public class ExecuteInEditModeAttribute : Attribute
{
    /// <summary>
    /// Initializes a new instance of the ExecuteInEditModeAttribute.
    /// </summary>
    public ExecuteInEditModeAttribute()
    {
    }
}
