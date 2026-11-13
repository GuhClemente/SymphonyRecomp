using RecompOne.Runtime.Events;
using RecompOne.Runtime.Host.Window;

namespace Recompiled;

public static class QualityOfLifeMenu
{
    public static void Register()
    {
        Event.AddListener<RuntimeReadyEvent>(_ => QualityOfLife.Load());
        foreach (var option in QualityOfLifeOptions.All)
            option.Changed += _ => QualityOfLife.Load();
        PanelManager.Register(new QualityOfLifePanel());
        PanelManager.Register(new FastTravelPanel());
        PanelManager.Register(new SpellWheelPanel());
        PanelManager.Register(new BestiaryPanel());
        PanelManager.Register(new AchievementsPanel());
        PanelManager.Register(new BossRespawnPanel());
        PanelManager.Register(new HardcoreModePanel());
        PanelManager.Register(new Recompiled.Netplay.MultiplayerPanel());

        MenuRegistry.Menu("menu.misc").After("menu.mods")
            .Panel<QualityOfLifePanel>("panel.qol")
            .Panel<FastTravelPanel>("panel.fast_travel")
            .Panel<SpellWheelPanel>("panel.spell_wheel")
            .Panel<BestiaryPanel>("panel.bestiary")
            .Panel<AchievementsPanel>("panel.achievements")
            .Panel<BossRespawnPanel>("panel.boss_respawn")
            .Panel<HardcoreModePanel>("panel.difficulty")
            .Panel<Recompiled.Netplay.MultiplayerPanel>("panel.multiplayer");
    }
}
