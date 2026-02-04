using GrapeEngine.Math;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems;
using GrapeEngine.Scripting.Systems.Attributes;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Events;
using System.Collections.Generic;


namespace EchoesBelow.Scripts.MarineSnowSystem;

[Component] public record struct MS_SpawnerComponent(bool start, float spawnInterval, int toSpawn, float timer);
[System(SystemGroup.Update, SystemRunMode.PlayOnly)]
public class MS_Spawner : SystemBase
{
    
    //toSpawn is formatted as 10000000, the 7 0's after 1 represent the different MS particles
    protected override void OnCreate()
    {
        Log("System MS_Spawner initialized");
    }
    private bool OnStart(ref bool startBool)
    {
        if (startBool == true) return true;
        startBool = true;
        //Todo



        //End of Start
        return true;
    }
    protected override void OnUpdate()
    {
        foreach(var gameObject in World!.Query<MS_SpawnerComponent>())
        {
            //A Pseudo Start function, called once per obj at runtime
            //This allows onStart to work
            bool start = gameObject.Component1.start;
            gameObject.Component1.start = OnStart(ref start);

            gameObject.Component1.timer += Time.DeltaTime;

            Entity entity = Entity.FromId(World!, gameObject.Entity.Id);

            //If we exceed the spawn interval
          
            if(gameObject.Component1.timer >= gameObject.Component1.spawnInterval)
            {
               
                //Reset the timer, prime it for the next interval
                gameObject.Component1.timer = 0;

                ShapeBox2D boundary = entity.GetComponent<ShapeBox2D>();
                LocalTransform transform = entity.GetComponent<LocalTransform>();
                //Set the x boundary min and max limits
                float xBoundaryMax = transform.Position.X + boundary.HalfExtents.X;
                float xBoundaryMin = transform.Position.X - boundary.HalfExtents.X;
                
                int msID = GMath.Random(0, 1) + 1;
               
                int spawnCount = GMath.Random(1, 2);
                for (int i = 0; i < spawnCount; i++)
                {
                    //Generate new spawn Coordinate
                    float xValue = GMath.Random(xBoundaryMin, xBoundaryMax);
                    Vector3 spawnPos = new Vector3(xValue, transform.Position.Y, 0);
                    
                    if(MS_Manager.instance.PullFromPool(msID, spawnPos) == MS_Manager.instance.emptyId) continue;
                }
            }
        }
    }

}
