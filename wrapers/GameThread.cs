using System.Collections.Concurrent;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Sotn;

//the game runs on its own thread and the panels draw on the window thread, so anything a panel wants
//that calls recompiled code (GameApi, Stages.Load, overlay loads) has to be posted here instead of
//being run from Draw(): GameApi.Call snapshots and restores the cpu context the game thread is using
public static class GameThread
{
    static readonly ConcurrentQueue<Action<CpuContext, IMemory>> Jobs = new();

    public static void Post(Action<CpuContext, IMemory> job) => Jobs.Enqueue(job);

    public static void Post(Action job) => Jobs.Enqueue((_, _) => job());

    //called once per frame from the game thread (QualityOfLife.Apply)
    public static void Drain(CpuContext c, IMemory m)
    {
        while (Jobs.TryDequeue(out var job))
        {
            try
            {
                job(c, m);
            }
            catch (Exception e)
            {
                Console.WriteLine($"[GameThread] job failed: {e}");
            }
        }
    }
}
