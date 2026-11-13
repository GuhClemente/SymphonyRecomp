using System;
using RecompOne.Runtime;

namespace Recompiled.Ultimate;

public enum SaturnBalanceMode
{
    Saturn = 0,
    PS1 = 1
}

public enum DubMode
{
    PS1Classic = 0,
    PSPEnglish = 1,
    Japanese = 2
}

public enum CharacterSelection
{
    Alucard = 0,
    RichterClassic = 1,
    RichterSaturnCoat = 2,
    SaturnMaria = 3,
    PSPMaria = 4
}

public enum BossMariaMode
{
    Default = 0,
    SaturnBoss = 1,
    PSPBoss = 2
}

public static class UltimateOptions
{
    public static bool UltimateEnabled = true;
    public static SaturnBalanceMode Balance = SaturnBalanceMode.Saturn;
    public static DubMode Dub = DubMode.PSPEnglish;
    public static CharacterSelection CharacterMode = CharacterSelection.Alucard;
    public static BossMariaMode BossMaria = BossMariaMode.SaturnBoss;
    public static bool GodSpeedShoes = true;
    public static bool ThirdHandHolder = true;
    public static bool SaturnStages = true;
    public static bool RainbowCapeCycle = true;

    public static void Load()
    {
        var v = RecompOne.Runtime.Runtime.View;
        UltimateEnabled = v.GetBool("Ultimate_Enabled", true);
        Balance = (SaturnBalanceMode)v.GetInt("Ultimate_Balance", (int)SaturnBalanceMode.Saturn);
        Dub = (DubMode)v.GetInt("Ultimate_Dub", (int)DubMode.PSPEnglish);
        CharacterMode = (CharacterSelection)v.GetInt("Ultimate_Character", (int)CharacterSelection.Alucard);
        BossMaria = (BossMariaMode)v.GetInt("Ultimate_BossMaria", (int)BossMariaMode.SaturnBoss);
        GodSpeedShoes = v.GetBool("Ultimate_GodSpeedShoes", true);
        ThirdHandHolder = v.GetBool("Ultimate_ThirdHandHolder", true);
        SaturnStages = v.GetBool("Ultimate_SaturnStages", true);
        RainbowCapeCycle = v.GetBool("Ultimate_RainbowCapeCycle", true);
    }

    public static void Save()
    {
        var v = RecompOne.Runtime.Runtime.View;
        v.SetBool("Ultimate_Enabled", UltimateEnabled);
        v.SetInt("Ultimate_Balance", (int)Balance);
        v.SetInt("Ultimate_Dub", (int)Dub);
        v.SetInt("Ultimate_Character", (int)CharacterMode);
        v.SetInt("Ultimate_BossMaria", (int)BossMaria);
        v.SetBool("Ultimate_GodSpeedShoes", GodSpeedShoes);
        v.SetBool("Ultimate_ThirdHandHolder", ThirdHandHolder);
        v.SetBool("Ultimate_SaturnStages", SaturnStages);
        v.SetBool("Ultimate_RainbowCapeCycle", RainbowCapeCycle);
        RecompOne.Runtime.Runtime.SaveView();
    }
}
