/* Start Header *****************************************************************/
/*!
\file   Strings.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Public API for string interning and resolution. Provides the entry point
for working with StringId in scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Central API for string interning and resolution.
/// 
/// The Strings class provides the primary interface for working with StringId
/// in ECS components. Instead of storing string references directly (which
/// violates ECS component rules), use StringId with the Strings API:
/// 
/// <code>
/// // Interning: Convert string -> StringId
/// StringId id = Strings.Intern("Player");
/// 
/// // Resolving: Convert StringId -> string
/// string? value = Strings.Resolve(id);
/// </code>
/// 
/// String interning is thread-safe and cached. The same string will always
/// produce the same StringId across multiple calls.
/// 
/// IMPORTANT: Do not clear the string table during hot reload. StringIds
/// stored in ECS components must remain valid across script reloads.
/// </summary>
public static class Strings
{
    /// <summary>
    /// Intern a string and return its StringId.
    /// 
    /// This function is idempotent: calling it multiple times with the same
    /// string will return the same StringId each time.
    /// 
    /// Thread-safe.
    /// </summary>
    /// <param name="value">The string to intern. Cannot be null.</param>
    /// <returns>A StringId that can be used to resolve the string later</returns>
    /// <exception cref="ArgumentNullException">If value is null</exception>
    public static StringId Intern(string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        return StringTable.Intern(value);
    }

    /// <summary>
    /// Resolve a StringId back to its original string.
    /// 
    /// Returns null if:
    /// - The StringId is invalid (Value == 0)
    /// - The StringId does not correspond to any interned string
    /// 
    /// Thread-safe.
    /// </summary>
    /// <param name="id">The StringId to resolve</param>
    /// <returns>The original string, or null if not found</returns>
    public static string? Resolve(StringId id)
        => StringTable.Resolve(id);
}
