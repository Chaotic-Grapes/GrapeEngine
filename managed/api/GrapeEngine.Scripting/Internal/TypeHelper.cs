/* Start Header *****************************************************************/
/*!
\file   TypeHelper.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Type reflection utilities for component validation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Reflection;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal;

/// <summary>
/// Helper utilities for type reflection and validation.
/// </summary>
internal static class TypeHelper
{
    /// <summary>
    /// Check if a type is truly unmanaged (can be used in P/Invoke and unsafe code).
    /// 
    /// This performs a runtime check that is more thorough than the compile-time
    /// 'where T : unmanaged' constraint, as it can catch edge cases where:
    /// - Type contains hidden managed references
    /// - Type uses records with reference semantics
    /// - Type has incorrect layout attributes
    /// </summary>
    /// <param name="type">The type to check</param>
    /// <returns>True if the type is unmanaged, false otherwise</returns>
    public static bool IsUnmanagedType(Type type)
    {
        // Try the reflection method first (most reliable)
        try
        {
            var method = typeof(Type).GetMethod("IsUnmanagedType", BindingFlags.NonPublic | BindingFlags.Instance);
            if (method != null)
            {
                var result = (bool)method.Invoke(type, null)!;
                return result;
            }
        }
        catch
        {
            // Fall through to other methods
        }

        // Fallback: try to use Marshal.SizeOf - if it works, the type is likely unmanaged
        try
        {
            Marshal.SizeOf(type);
            return true;
        }
        catch
        {
            // Type is not unmanaged
        }

        // Last resort: check if it has StructLayout attribute
        // This is a weak check but better than nothing
        return type.GetCustomAttribute<StructLayoutAttribute>() != null;
    }
}
