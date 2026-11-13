using System;
using System.Collections.Generic;
using System.Numerics;
using ImGuiNET;
using RecompOne.Runtime.Host.Window;
using Sotn;

namespace Recompiled;

public sealed class BestiaryPanel : IPanel
{
    public string Name => "Bestiary & Drop Compendium";
    public string TitleKey => "panel.bestiary";
    public bool IsOpen { get; set; }

    public record MonsterEntry(
        int Id,
        string Name,
        int Hp,
        int Exp,
        int Attack,
        int Defense,
        string CommonDrop,
        float CommonRate,
        string RareDrop,
        float RareRate,
        string Weakness,
        string Resistance,
        string Zone
    );

    private static string _search = "";
    private static int _categoryFilter = 0; // 0: All, 1: Normal Castle, 2: Inverted Castle, 3: Bosses

    public static readonly MonsterEntry[] Database =
    [
        // Bosses
        new(1, "Dracula", 600, 0, 30, 10, "None", 0f, "None", 0f, "Holy", "Dark, Fire", "Prologue / Throne Room"),
        new(2, "Slogra", 200, 200, 15, 4, "None", 0f, "None", 0f, "Fire", "Cut", "Alchemy Laboratory"),
        new(3, "Gaibon", 200, 200, 15, 3, "None", 0f, "None", 0f, "Ice, Holy", "Fire", "Alchemy Laboratory"),
        new(4, "Doppleganger10", 640, 500, 20, 5, "None", 0f, "None", 0f, "Poison", "Dark", "Outer Wall"),
        new(5, "Minotaur", 300, 400, 22, 6, "None", 0f, "None", 0f, "Poison, Dark", "Hit", "Colosseum"),
        new(6, "Werewolf", 260, 400, 24, 4, "None", 0f, "None", 0f, "Holy, Fire", "Dark", "Colosseum"),
        new(7, "Scylla", 200, 500, 25, 8, "None", 0f, "None", 0f, "Lightning, Fire", "Ice, Water", "Underground Caverns"),
        new(8, "Hippogryph", 800, 800, 30, 8, "None", 0f, "None", 0f, "Fire, Lightning", "Wind", "Royal Chapel"),
        new(9, "Olrox", 666, 2000, 35, 12, "None", 0f, "None", 0f, "Holy", "Dark", "Olrox's Quarters"),
        new(10, "Granfaloon (Legion)", 400, 4000, 32, 10, "None", 0f, "None", 0f, "Holy, Fire", "Dark", "Catacombs"),
        new(11, "Cerberus", 800, 1500, 34, 12, "None", 0f, "None", 0f, "Ice", "Fire", "Abandoned Mine"),
        new(12, "Richter Belmont", 400, 0, 40, 18, "None", 0f, "None", 0f, "Dark, Stone", "Holy", "Castle Keep"),
        new(13, "Galamoth", 6666, 9999, 120, 60, "None", 0f, "Ruby Circlet", 100f, "Dark", "Lightning, Holy", "Floating Catacombs"),
        new(14, "Beelzebub", 2000, 4444, 55, 20, "None", 0f, "Ring of Arcana", 100f, "Fire, Holy", "Poison, Dark", "Necromancy Laboratory"),
        new(15, "Death", 4400, 4444, 60, 24, "None", 0f, "Eye of Vlad", 100f, "Holy", "Dark", "Cave (Inverted Mine)"),

        // High Value Farm Monsters & Weapons
        new(16, "Schmoo", 50, 1000, 40, 2, "Ramen", 12.5f, "Crissaegrim", 1.5f, "Fire, Holy", "Dark", "Forbidden Library"),
        new(17, "Cloaked Knight", 120, 1200, 50, 20, "Knight Shield", 6.0f, "Heaven Sword", 1.2f, "Holy", "Cut, Hit", "Clock Tower / Reverse"),
        new(18, "Nova Skeleton", 180, 777, 45, 10, "Monster Vial 3", 8.0f, "Terminus Est", 2.0f, "Holy, Fire", "Dark", "Inverted Castle"),
        new(19, "Malachi", 450, 666, 65, 15, "Zircon", 6.0f, "Dark Blade", 2.5f, "Holy", "Dark", "Anti-Chapel / Outer Wall"),
        new(20, "Spectral Sword", 90, 400, 30, 8, "Bastard Sword", 5.0f, "Gurthang", 2.0f, "Fire, Holy", "Cut", "Royal Chapel / Anti-Chapel"),
        new(21, "Cave Troll", 88, 300, 32, 6, "Pork Bun", 10.0f, "Neutron Bomb", 3.0f, "Fire, Holy", "Dark", "Underground Caverns"),
        new(22, "Bloody Zombie", 35, 15, 12, 1, "Cloth Tunic", 4.0f, "Basilard", 1.0f, "Holy, Fire", "Dark", "Castle Entrance"),
        new(23, "Warg", 32, 12, 8, 2, "None", 0f, "None", 0f, "Fire", "Ice", "Castle Entrance"),
        new(24, "Skeleton", 10, 10, 6, 0, "Monster Vial 1", 5.0f, "Red Rust", 2.0f, "Holy, Fire", "Dark", "Alchemy Laboratory"),
        new(25, "Blood Skeleton", 9, 0, 18, 999, "None", 0f, "None", 0f, "Holy", "Invulnerable", "Catacombs / Chapel"),
        new(26, "Flea Man", 10, 17, 10, 0, "High Potion", 2.0f, "Cheese", 6.0f, "Cut, Hit", "None", "Library / Chapel"),
        new(27, "Dhuron", 90, 80, 20, 5, "Rapier", 4.0f, "Combat Knife", 6.0f, "Fire", "None", "Long Library"),
        new(28, "Spellbook", 26, 40, 16, 2, "Pentagram", 4.0f, "Magic Missile", 8.0f, "Fire", "Holy", "Long Library"),
        new(29, "Mudman", 15, 20, 14, 0, "None", 0f, "None", 0f, "Fire, Ice", "Earth", "Long Library (Lesser Demon)"),
        new(30, "Lesser Demon", 160, 100, 24, 8, "Shortcake", 8.0f, "Obsidian Sword", 2.0f, "Holy", "Dark", "Long Library / Laboratory"),
        new(31, "Ctulhu", 200, 150, 28, 12, "Uncurse", 10.0f, "Talwar", 3.0f, "Holy, Fire", "Dark", "Marble Gallery"),
        new(32, "Plate Lord", 90, 110, 26, 14, "Iron Ball", 5.0f, "Plate Lord Armor", 2.0f, "Lightning", "Cut, Hit", "Marble Gallery / Chapel"),
        new(33, "Armor Lord", 80, 120, 30, 16, "Saber", 6.0f, "Helmet", 4.0f, "Lightning", "Cut, Hit", "Outer Wall"),
        new(34, "Sword Lord", 140, 240, 36, 18, "Claymore", 5.0f, "Bekatowa", 3.0f, "Lightning", "Cut", "Clock Tower"),
        new(35, "Valhalla Knight", 220, 800, 48, 22, "Zweihander", 4.0f, "Alucard Mail", 1.0f, "Holy", "Cut, Hit", "Reverse Outer Wall"),
        new(36, "Guardian", 500, 1500, 95, 120, "God's Garb", 1.5f, "Great Sword", 3.0f, "Dark", "Holy, All", "Black Marble Gallery"),
        new(37, "Azaghal", 330, 900, 54, 25, "Mourneblade", 2.0f, "Icebrand", 3.5f, "Fire", "Ice", "Reverse Colosseum"),
        new(38, "Dodo Bird", 100, 777, 10, 0, "Heart Refresh", 6.0f, "Runesword", 1.0f, "Fire, Cut", "None", "Reverse Entrance"),
        new(39, "Tin Man", 48, 888, 55, 30, "Lunch B", 10.0f, "Mablung Sword", 1.5f, "Lightning", "Cut, Hit", "Death Wing's Lair"),
        new(40, "Paranthropus", 100, 150, 25, 4, "Meal Ticket", 8.0f, "Gauntlet", 4.0f, "Fire, Holy", "Dark", "Outer Wall"),
    ];

    public void Draw()
    {
        ImGui.SetNextWindowSize(new Vector2(520, 600), ImGuiCond.FirstUseEver);
        bool open = IsOpen;
        if (!ImGui.Begin(this.Title(), ref open))
        {
            IsOpen = open;
            ImGui.End();
            return;
        }

        if (ImGui.BeginTabBar("bestiary_tabs"))
        {
            if (ImGui.BeginTabItem("Inimigos em Tela (Scanner)"))
            {
                DrawLiveScanner();
                ImGui.EndTabItem();
            }

            if (ImGui.BeginTabItem("Compêndio de Monstros"))
            {
                DrawCompendium();
                ImGui.EndTabItem();
            }

            ImGui.EndTabBar();
        }

        IsOpen = open;
        ImGui.End();
    }

    private void DrawLiveScanner()
    {
        if (RecompOne.Runtime.Runtime.Mem == null || !Cheats.InPlay())
        {
            ImGui.TextWrapped(Localization.T("common.not_in_play"));
            return;
        }

        int foundCount = 0;

        ImGui.TextColored(new Vector4(0.2f, 0.9f, 0.3f, 1f), "Radar de Inimigos Ativos na Sala:");
        ImGui.Separator();

        ImGui.BeginChild("live_scanner_child", Vector2.Zero, ImGuiChildFlags.Borders);

        for (int i = 1; i < Entities.Count; i++)
        {
            var ent = Entities.At(i);
            if (!ent.IsValid || ent.HitPoints <= 0) continue;
            if ((ent.Flags & (int)EntityFlags.NotAnEnemy) != 0) continue;

            foundCount++;
            ImGui.PushID(i);

            ImGui.TextColored(new Vector4(1f, 0.8f, 0.2f, 1f), $"Entidade #{i} (ID: {ent.EnemyId})");
            ImGui.SameLine();
            ImGui.Text($"HP: {ent.HitPoints} | ATK: {ent.Attack}");

            float hpWidth = Math.Clamp((float)ent.HitPoints / 500f, 0.05f, 1f);
            ImGui.ProgressBar(hpWidth, new Vector2(-1, 14), $"{ent.HitPoints} HP");

            ImGui.TextDisabled($"Pos: ({ent.PosX}, {ent.PosY}) | Prioridade: {ent.ZPriority}");
            ImGui.Separator();

            ImGui.PopID();
        }

        if (foundCount == 0)
        {
            ImGui.TextDisabled("Nenhum inimigo detectado na tela no momento.");
        }

        ImGui.EndChild();
    }

    private void DrawCompendium()
    {
        ImGui.SetNextItemWidth(240);
        ImGui.InputTextWithHint("##bestiary_search", "Buscar monstro ou drop...", ref _search, 32);
        ImGui.SameLine();

        string[] cats = ["Todos", "Castelo Normal", "Castelo Invertido", "Chefes"];
        ImGui.SetNextItemWidth(160);
        ImGui.Combo("##cat_combo", ref _categoryFilter, cats, cats.Length);

        ImGui.Spacing();

        ImGui.BeginChild("bestiary_list_child", Vector2.Zero, ImGuiChildFlags.Borders);

        foreach (var m in Database)
        {
            // Category filter
            if (_categoryFilter == 1 && (m.Zone.Contains("Inverted") || m.Zone.Contains("Reverse") || m.Zone.Contains("Boss") || m.Zone.Contains("Prologue") || m.Zone.Contains("Floating") || m.Zone.Contains("Forbidden") || m.Zone.Contains("Anti-Chapel") || m.Zone.Contains("Death Wing")))
                continue;
            if (_categoryFilter == 2 && !(m.Zone.Contains("Inverted") || m.Zone.Contains("Reverse") || m.Zone.Contains("Floating") || m.Zone.Contains("Forbidden") || m.Zone.Contains("Anti-Chapel") || m.Zone.Contains("Death Wing") || m.Zone.Contains("Black Marble")))
                continue;
            if (_categoryFilter == 3 && !m.Zone.Contains("Boss") && m.Id > 15)
                continue;

            // Search filter
            if (!string.IsNullOrWhiteSpace(_search))
            {
                bool matchName = m.Name.Contains(_search, StringComparison.OrdinalIgnoreCase);
                bool matchDrop1 = m.CommonDrop.Contains(_search, StringComparison.OrdinalIgnoreCase);
                bool matchDrop2 = m.RareDrop.Contains(_search, StringComparison.OrdinalIgnoreCase);
                bool matchZone = m.Zone.Contains(_search, StringComparison.OrdinalIgnoreCase);
                if (!matchName && !matchDrop1 && !matchDrop2 && !matchZone)
                    continue;
            }

            ImGui.PushID(m.Id);

            if (ImGui.CollapsingHeader($"{m.Name}  (HP: {m.Hp} | EXP: {m.Exp})"))
            {
                ImGui.Columns(2, "bestiary_cols", false);
                ImGui.SetColumnWidth(0, 200);

                ImGui.TextColored(new Vector4(0.4f, 0.8f, 1f, 1f), "Atributos:");
                ImGui.Text($"HP: {m.Hp}");
                ImGui.Text($"EXP: {m.Exp}");
                ImGui.Text($"Ataque: {m.Attack}");
                ImGui.Text($"Defesa: {m.Defense}");
                ImGui.TextDisabled($"Local: {m.Zone}");

                ImGui.NextColumn();

                ImGui.TextColored(new Vector4(1f, 0.85f, 0.3f, 1f), "Tabela de Drops:");
                if (m.CommonDrop != "None")
                    ImGui.Text($"Comum: {m.CommonDrop} ({m.CommonRate:0.0}%)");
                else
                    ImGui.TextDisabled("Comum: Nenhum");

                if (m.RareDrop != "None")
                    ImGui.TextColored(new Vector4(0.3f, 1f, 0.5f, 1f), $"Raro: {m.RareDrop} ({m.RareRate:0.0}%)");
                else
                    ImGui.TextDisabled("Raro: Nenhum");

                ImGui.Spacing();
                ImGui.TextColored(new Vector4(1f, 0.4f, 0.4f, 1f), $"Fraqueza: {m.Weakness}");
                ImGui.TextColored(new Vector4(0.7f, 0.7f, 0.7f, 1f), $"Resistência: {m.Resistance}");

                ImGui.Columns(1);
                ImGui.Separator();
            }

            ImGui.PopID();
        }

        ImGui.EndChild();
    }
}
