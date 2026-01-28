/* Start Header *****************************************************************/
/*!
\file   PreserveAttribute.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Attribute to mark fields for preservation during hot reload.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// Mark a field with this attribute to preserve its value across hot reloads.
/// Only works with serializable types (primitives, strings, and complex types
/// with default parameterless constructors).
/// </summary>
[AttributeUsage(AttributeTargets.Field, AllowMultiple = false, Inherited = false)]
public class PreserveAttribute : Attribute
{
    /// <summary>
    /// Initializes a new instance of the PreserveAttribute.
    /// </summary>
    public PreserveAttribute()
    {
    }
}


