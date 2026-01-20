/* Start Header *****************************************************************/
/*!
\file   ISystemJob.cs
\brief  Optional interface for managed systems that return a JobHandle
        from their update method. Native host can call the job-capable
        entrypoint to obtain the native job handle.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Job;

namespace GrapeEngine.Scripting.Systems;

/// <summary>
/// Optional interface. If a system implements this, the native bridge will
/// call `OnUpdateWithJob` and return the underlying native job handle.
/// This allows systems to schedule jobs and have the engine chain/wait on them.
/// </summary>
public interface ISystemJob : ISystem
{
    /// <summary>
    /// Called each frame; returns a JobHandle representing scheduled work.
    /// The native bridge will extract the native handle and return it to the caller.
    /// </summary>
    JobHandle OnUpdateWithJob(World world, JobHandle? dependency);
}
