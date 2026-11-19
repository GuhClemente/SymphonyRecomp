namespace Recompiled;

// Pure HUD rules with no game or ImGui dependencies, so the test project can compile and check them.

public enum ThirdHandChoice
{
    None,
    Uncurse,
    Antivenom,
    Elixir,
    HighPotion,
    Potion,
    Food,
    MannaPrism,
}

/// <summary>Player state and hand-item stock the Third Hand decides from. Food is the total of all food items.</summary>
public readonly record struct ThirdHandState(
    int Hp, int MaxHp, int Mp, int MaxMp,
    bool Cursed, bool Poisoned,
    int Uncurse, int Antivenom, int Elixir, int HighPotion, int Potion, int Food, int MannaPrism);

public static class HudLogic
{
    public const int HighPotionHeal = 100;
    public const int PotionHeal = 50;
    public const int FoodHeal = 35;

    /// <summary>The item the Third Hand uses right now, or None when nothing would help.</summary>
    public static ThirdHandChoice ChooseThirdHand(in ThirdHandState s)
    {
        if (s.Cursed && s.Uncurse > 0) return ThirdHandChoice.Uncurse;
        if (s.Poisoned && s.Antivenom > 0) return ThirdHandChoice.Antivenom;

        int missingHp = s.MaxHp - s.Hp;
        if (missingHp > 0)
        {
            if (s.Hp <= s.MaxHp * 0.35f && s.Elixir > 0) return ThirdHandChoice.Elixir;
            if (missingHp >= 60 && s.HighPotion > 0) return ThirdHandChoice.HighPotion;
            if (s.Potion > 0) return ThirdHandChoice.Potion;
            if (s.Food > 0) return ThirdHandChoice.Food;
        }

        if (s.Mp <= s.MaxMp * 0.25f && s.MannaPrism > 0) return ThirdHandChoice.MannaPrism;
        return ThirdHandChoice.None;
    }

    /// <summary>What the HUD slot shows: the item that would be used now, or else the first one in stock
    /// in the order the Third Hand reaches for them.</summary>
    public static ThirdHandChoice PreviewThirdHand(in ThirdHandState s)
    {
        var now = ChooseThirdHand(s);
        if (now != ThirdHandChoice.None) return now;
        if (s.Potion > 0) return ThirdHandChoice.Potion;
        if (s.HighPotion > 0) return ThirdHandChoice.HighPotion;
        if (s.Food > 0) return ThirdHandChoice.Food;
        if (s.Elixir > 0) return ThirdHandChoice.Elixir;
        if (s.MannaPrism > 0) return ThirdHandChoice.MannaPrism;
        if (s.Uncurse > 0) return ThirdHandChoice.Uncurse;
        if (s.Antivenom > 0) return ThirdHandChoice.Antivenom;
        return ThirdHandChoice.None;
    }

    public static int StockOf(in ThirdHandState s, ThirdHandChoice c) => c switch
    {
        ThirdHandChoice.Uncurse => s.Uncurse,
        ThirdHandChoice.Antivenom => s.Antivenom,
        ThirdHandChoice.Elixir => s.Elixir,
        ThirdHandChoice.HighPotion => s.HighPotion,
        ThirdHandChoice.Potion => s.Potion,
        ThirdHandChoice.Food => s.Food,
        ThirdHandChoice.MannaPrism => s.MannaPrism,
        _ => 0,
    };

    /// <summary>HP restored by a healing choice (Elixir fills it; status cures and Manna Prism restore none).</summary>
    public static int HealAmount(ThirdHandChoice c, int missingHp) => c switch
    {
        ThirdHandChoice.Elixir => missingHp,
        ThirdHandChoice.HighPotion => System.Math.Min(HighPotionHeal, missingHp),
        ThirdHandChoice.Potion => System.Math.Min(PotionHeal, missingHp),
        ThirdHandChoice.Food => System.Math.Min(FoodHeal, missingHp),
        _ => 0,
    };

    /// <summary>Castle map cell of a player: the room's origin cell (g_Tilemap left/top, as in Stages.RoomX)
    /// plus the 256-pixel cell the player stands in inside that room (world position >> 8).</summary>
    public static int MapCell(int roomOrigin, int worldPos) => roomOrigin + (worldPos >> 8);

    /// <summary>Screen pixel of a world-space coordinate for a camera scrolled to <paramref name="scroll"/>.</summary>
    public static float WorldToScreen(float worldPos, int scroll) => worldPos - scroll;
}
