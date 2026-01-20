/* Start Header *****************************************************************/
/*!
\file   SystemAdapter.cs
\brief  Lightweight managed adapter for `ISystem` instances. Provides a
        simple call-through surface that can be used by hosting code.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Systems;

internal sealed class SystemAdapter
{
    private readonly ISystem _system;

    public SystemAdapter(ISystem system)
    {
        _system = system ?? throw new System.ArgumentNullException(nameof(system));
    }

    public void InvokeOnCreate(World world)
    {
        _system.OnCreate(world);
    }

    public void InvokeOnUpdate(World world)
    {
        _system.OnUpdate(world);
    }

    public void InvokeOnDestroy(World world)
    {
        _system.OnDestroy(world);
    }
}
