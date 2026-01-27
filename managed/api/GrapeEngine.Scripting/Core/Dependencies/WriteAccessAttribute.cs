/* Start Header *****************************************************************/
/*!
\file   WriteAccessAttribute.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Attribute to mark a component type parameter as having write access.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.Dependencies;

/// <summary>
/// Mark a component type parameter as having write access.
/// Indicates that the system may read and modify this component type.
/// Only one system can write to a component per system group.
/// 
/// This attribute is used for:
/// - Dependency analysis (detecting write conflicts)
/// - Parallel scheduling (writers must be serialized)
/// - Optimization hints (component data is writable by this system)
/// 
/// Note: Use [WriteAccess] even if the system only writes without reading.
/// The dependency system will detect the exclusive write requirement.
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
///         // Cannot run in parallel with other systems that write Transform
///         foreach (var result in world.Query&lt;Position, Velocity&gt;())
///         {
///             result.Component1.Value += result.Component2.Value * deltaTime;
///         }
///     }
/// }
/// </code>
/// </summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = false)]
public class WriteAccessAttribute<T> : Attribute
    where T : unmanaged
{
    /// <summary>
    /// Initializes a new instance of the WriteAccessAttribute for a specific component type.
    /// </summary>
    public WriteAccessAttribute()
    {
        AccessMode = ComponentAccessMode.Write;
    }

    /// <summary>
    /// Gets the component type marked as having write access.
    /// </summary>
    public Type ComponentType => typeof(T);

    /// <summary>
    /// Gets the access mode (always Write for this attribute).
    /// </summary>
    public ComponentAccessMode AccessMode { get; }
}

