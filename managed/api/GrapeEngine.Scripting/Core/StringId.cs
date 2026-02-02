/* Start Header *****************************************************************/
/*!
\file   StringId.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
String identifier type for ECS components. Provides a blittable alternative to
string references that can safely live in ECS chunks.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// An unmanaged, blittable identifier for interned strings.
/// 
/// StringId is used in ECS components to reference string data without storing
/// managed string references directly in component memory. This allows:
/// - Components to remain unmanaged and trivially copyable
/// - Safe memcpy semantics for ECS chunk storage
/// - Proper C++ interop without marshalling overhead
/// - Hot reload without breaking string references
/// 
/// Usage:
/// <code>
/// public struct Name
/// {
///     public StringId Value;
/// }
/// 
/// entity.AddComponent(new Name
/// {
///     Value = Strings.Intern("Player")
/// });
/// 
/// string name = Strings.Resolve(entity.GetComponent&lt;Name&gt;().Value);
/// </code>
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct StringId : IEquatable<StringId>
{
    /// <summary>
    /// The internal identifier. 0 = invalid/null string.
    /// </summary>
    public readonly uint Value;

    /// <summary>
    /// Internal constructor. Use Strings.Intern() to create StringIds.
    /// </summary>
    internal StringId(uint value) => Value = value;

    /// <summary>
    /// Check if this StringId is valid (non-zero).
    /// </summary>
    public bool IsValid => Value != 0;

    public bool Equals(StringId other) => Value == other.Value;
    public override bool Equals(object? obj) => obj is StringId other && Equals(other);
    public override int GetHashCode() => (int)Value;

    public static bool operator ==(StringId a, StringId b) => a.Value == b.Value;
    public static bool operator !=(StringId a, StringId b) => a.Value != b.Value;

    /// <summary>
    /// Convert StringId to its string representation.
    /// Returns the resolved string, or "&lt;StringId:N&gt;" if the ID is invalid or not found.
    /// </summary>
    public override string ToString()
        => Strings.Resolve(this) ?? $"<StringId:{Value}>";
}
