/* Start Header *****************************************************************/
/*!
\file   ReadOnlyAttribute.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Attribute to mark a component type parameter as read-only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.Dependencies;

/// <summary>
/// Mark a component type parameter as read-only.
/// Indicates that the system will not modify this component type.
/// Multiple systems can read the same component concurrently.
/// 
/// This attribute is used for:
/// - Dependency analysis (detecting read/write conflicts)
/// - Parallel scheduling (multiple readers can run together)
/// - Optimization hints (component data is immutable for this system)
/// 
/// Example:
/// <code>
/// [ReadOnly&lt;Velocity&gt;]
/// [WriteAccess&lt;Transform&gt;]
/// public class MovementSystem : ISystem
/// {
///     public void OnUpdate(World world, float deltaTime)
///     {
///         // Declares: reads Velocity, writes Transform
///         // Can run in parallel with other systems that only read Transform
///         foreach (var result in world.Query&lt;Position, Velocity&gt;())
///         {
///             result.Component1.Value += result.Component2.Value * deltaTime;
///         }
///     }
/// }
/// </code>
/// </summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = false)]
public class ReadOnlyAttribute<T> : Attribute
    where T : unmanaged
{
    /// <summary>
    /// Initializes a new instance of the ReadOnlyAttribute for a specific component type.
    /// </summary>
    public ReadOnlyAttribute()
    {
        AccessMode = ComponentAccessMode.Read;
    }

    /// <summary>
    /// Gets the component type marked as read-only.
    /// </summary>
    public Type ComponentType => typeof(T);

    /// <summary>
    /// Gets the access mode (always Read for this attribute).
    /// </summary>
    public ComponentAccessMode AccessMode { get; }
}
