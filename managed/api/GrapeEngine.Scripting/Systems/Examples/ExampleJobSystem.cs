/* Start Header *****************************************************************/
/*!
\file   ExampleJobSystem.cs
\brief  Example system demonstrating `ISystemJob` usage by scheduling a trivial job
        and returning its native handle to the engine.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Job;

namespace GrapeEngine.Scripting.Systems.Examples;

public sealed class ExampleJobSystem : SystemBase, ISystemJob
{
    protected override void OnCreate()
    {
        // initialization
    }

    protected override void OnUpdate()
    {
        // legacy per-frame path (no-op)
    }

    protected override void OnDestroy()
    {
        // cleanup
    }

    // ISystemJob implementation: schedule a trivial job and return its handle
    public JobHandle OnUpdateWithJob(World world, JobHandle? dependency)
    {
        var job = new TrivialJob();
        return world.JobManager.Schedule(job, dependency, priority: 0);
    }

    private class TrivialJob : IJob
    {
        public CommandBuffer? Buffer { get; set; }

        public void Execute()
        {
            // small workload or no-op
        }

        public string GetJobName() => "ExampleJobSystem.TrivialJob";
    }
}
