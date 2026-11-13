using System;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;

namespace Recompiled.Ultimate;

public static class UltimateMode
{
    private static bool _registered = false;

    public static void Register()
    {
        if (_registered) return;
        _registered = true;

        Event.AddListener<RuntimeReadyEvent>(_ => Load());

        var panel = new UltimatePanel();
        PanelManager.Register(panel);

        // Add to "menu.mods" and "menu.misc"
        MenuRegistry.Menu("menu.mods").Panel<UltimatePanel>("panel.ultimate");
        MenuRegistry.Menu("menu.misc").Panel<UltimatePanel>("panel.ultimate");
    }

    public static void Load()
    {
        UltimateOptions.Load();
    }

    public static void Save()
    {
        UltimateOptions.Save();
    }

    public static void Update(CpuContext c, IMemory m)
    {
        if (!UltimateOptions.UltimateEnabled) return;

        SaturnBalance.Update(m);
        GodSpeedShoes.Update(c, m);
        SaturnItems.Update(c, m);
        SaturnStages.Update(c, m);
        MariaCharacter.Update(c, m);
    }
}
