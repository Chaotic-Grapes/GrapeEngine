/* Start Header *****************************************************************/
/*!
\file   ComponentAccessAttributes.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Attributes for declaring component access patterns (read-only vs write access).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting;

/// <summary>
/// Mark a component type parameter as read-only.
/// Indicates that the system will not modify this component type.
/// Used for dependency analysis and potential optimization.
/// 
/// Example:
/// <code>
/// [ReadOnly&lt;Velocity&gt;]
/// public class MySystem : ISystem
/// {
///     public void OnUpdate(World world, float deltaTime)
///     {
///         foreach (var (entity, pos, vel) in world.Query&lt;Position, Velocity&gt;())
///         {
///             // Can read vel, but not modify it
///             var speed = vel.Value.Length();
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
    }

    /// <summary>
    /// Gets the component type marked as read-only.
    /// </summary>
    public Type ComponentType => typeof(T);
}

/// <summary>
/// Mark a component type parameter as having write access.
/// Indicates that the system may read and modify this component type.
/// This is the default behavior; use [ReadOnly] to explicitly mark read-only access.
/// 
/// Example:
/// <code>
/// [WriteAccess&lt;Position&gt;]
/// public class MovementSystem : ISystem
/// {
///     public void OnUpdate(World world, float deltaTime)
///     {
///         foreach (var (entity, pos, vel) in world.Query&lt;Position, Velocity&gt;())
///         {
///             pos.Value += vel.Value * deltaTime;  // Modifying pos
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
    }

    /// <summary>
    /// Gets the component type marked as having write access.
    /// </summary>
    public Type ComponentType => typeof(T);
}
