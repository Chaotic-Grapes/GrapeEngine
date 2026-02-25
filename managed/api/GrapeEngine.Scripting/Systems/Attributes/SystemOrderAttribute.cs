/* Start Header *****************************************************************/
/*!
\file   SystemOrderAttribute.cs
\author Muhammad Nur Fadzly Bin Zulkifli
\par    muhammadnurfadzly.b@digipen.edu

\brief 
Attribute for declaring system execution order.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Systems.Attributes;

/// <summary>
/// Specifies a system execution order within its group (lower = earlier).
/// </summary>
[AttributeUsage(AttributeTargets.Class, Inherited = true)]
public sealed class SystemOrderAttribute(int order) : Attribute
{
    /// <summary>
    /// Execution order within the group.
    /// </summary>
    public int Order { get; } = order;
}
