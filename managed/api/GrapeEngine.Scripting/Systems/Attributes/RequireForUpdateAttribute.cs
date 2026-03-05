/* Start Header *****************************************************************/
/*!
\file   RequireForUpdateAttribute.cs
\brief  Attribute that requires matching entities before a system updates.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Systems.Attributes;

/// <summary>
/// Require that at least one entity with component <typeparamref name="T"/> exists
/// before the system is considered runnable.
/// 
/// Multiple attributes combine with AND semantics. Example:
/// [RequireForUpdate&lt;Transform&gt;]
/// [RequireForUpdate&lt;Velocity&gt;]
/// requires at least one entity that has both Transform and Velocity.
/// </summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = true)]
public sealed class RequireForUpdateAttribute<T> : Attribute
    where T : unmanaged
{
    /// <summary>
    /// Component type required for this system to run.
    /// </summary>
    public Type ComponentType => typeof(T);
}

