using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled;

public static class SkipCdRooms
{
    public static bool Enabled = false;

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        Enabled = v.GetBool("QolSkipCdRooms", false);
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetBool("QolSkipCdRooms", Enabled);
        RecompOne.Runtime.Runtime.SaveView();
    }

    public static void Update(CpuContext c, IMemory m)
    {
        // Safe: never advance state while CD is actively streaming stage data (prevents file/audio corruption)
        if (!Enabled || !Game.Available || !Game.InGame || Game.IsLoading) return;
    }
}
