using System;
using RecompOne.Runtime.Events;
using RecompOne.Runtime.Memory;
using Recompiled.Netplay;
using Sotn;

namespace Recompiled.Ultimate;

/// <summary>
/// SOTN_TEST_HUD=1 (with SOTN_TEST_MARIA=1): checks the HUD / QoL fixes inside the running game —
/// Third Hand (F, the C key, presses while paused), minimap cells, the partner's screen position and
/// the Player Select keys and input block. One "[HUDTEST] PASS|FAIL" line per check, then a summary,
/// and "[HUDTEST] SHOT name" before a pause so a window capture can be taken outside the process.
/// </summary>
public static partial class CharacterTest
{
    private static int _hudPass, _hudFail;
    private static int _probeX;

    private static void Check(bool ok, string name, string details = "")
    {
        if (ok) _hudPass++; else _hudFail++;
        Console.WriteLine($"[HUDTEST] {(ok ? "PASS" : "FAIL")} {name}{(details.Length > 0 ? " | " + details : "")}");
    }

    private static void Key(Silk.NET.Input.Key key) =>
        Event.Dispatch(new KeyboardEvent { Key = (int)key, Pressed = true, Repeat = false });

    private static int WorldX(IMemory m) => m.ReadU16(Stages.PlayerWorldXAddr);
    private static int WorldY(IMemory m) => m.ReadU16(Stages.PlayerWorldYAddr);

    private static void Bag(int elixir, int high, int potion)
    {
        foreach (var it in new[] { HandItem.Apple, HandItem.Banana, HandItem.Grapes, HandItem.Strawberry,
                                   HandItem.Pineapple, HandItem.Cheesecake, HandItem.Shortcake,
                                   HandItem.Uncurse, HandItem.Antivenom, HandItem.MannaPrism })
            Inventory.SetHandCount(it, 0);
        Inventory.SetHandCount(HandItem.Elixir, elixir);
        Inventory.SetHandCount(HandItem.HighPotion, high);
        Inventory.SetHandCount(HandItem.Potion, potion);
    }

    private static void HudShot(string name)
    {
        Do((_, _) => Console.WriteLine($"[HUDTEST] SHOT {name}"));
        Wait(150);
    }

    private static void BuildHudScript()
    {
        Do((_, _) => { _who = "hud"; Console.WriteLine("[HUDTEST] ===== inicio ====="); });
        Wait(60);
        WaitIdle();
        HudShot("minimapa_e_terceira_mao");

        // ---- Third Hand: F with critical HP uses the Elixir ----
        Do((_, _) =>
        {
            _topUpHp = false;
            Player.Mp = Player.MpMax;
            Bag(elixir: 1, high: 1, potion: 2);
            Player.Hp = Math.Max(1, Player.HpMax * 30 / 100);
            var (name, _) = ThirdHandManager.GetBestAvailableConsumable();
            Check(name == "Elixir", "Terceira Mão HUD mostra o Elixir com HP crítico", $"slot={name}");
            Key(Silk.NET.Input.Key.T);
        });
        Wait(5);
        Do((_, _) =>
        {
            Check(Player.Hp == Player.HpMax, "T com HP crítico enche o HP", $"hp={Player.Hp}/{Player.HpMax}");
            Check(Inventory.GetHandCount(HandItem.Elixir) == 0, "T gastou exatamente o Elixir",
                $"elixir={Inventory.GetHandCount(HandItem.Elixir)} high={Inventory.GetHandCount(HandItem.HighPotion)} potion={Inventory.GetHandCount(HandItem.Potion)}");
        });
        Wait(40); // past the 0.5 s cooldown

        // ---- the slot shows what F will use; C does not consume anything ----
        Do((_, _) =>
        {
            Player.Hp = Player.HpMax - 20;
            var (name, count) = ThirdHandManager.GetBestAvailableConsumable();
            Check(name == "Potion" && count == 2, "Slot mostra a Potion com dano leve", $"slot={name} x{count}");
            Key(Silk.NET.Input.Key.C);
        });
        Wait(10);
        Do((_, _) => Check(Inventory.GetHandCount(HandItem.Potion) == 2 && Inventory.GetHandCount(HandItem.HighPotion) == 1,
            "Tecla C não gasta consumível", $"potion={Inventory.GetHandCount(HandItem.Potion)}"));
        Wait(90);
        WaitIdle();

        Do((_, _) => { Player.Hp = Player.HpMax - 20; Key(Silk.NET.Input.Key.T); });
        Wait(5);
        Do((_, _) => Check(Inventory.GetHandCount(HandItem.Potion) == 1 && Player.Hp == Player.HpMax,
            "T com dano leve usa a Potion", $"potion={Inventory.GetHandCount(HandItem.Potion)} hp={Player.Hp}/{Player.HpMax}"));
        Wait(40);

        // ---- T pressed while paused must not fire later ----
        Do((_, _) => Player.Hp = Player.HpMax - 20);
        Tap(Button.Start);
        Wait(40);
        Do((_, _) =>
        {
            Console.WriteLine($"[HUDTEST] pausado: HasControl={Player.HasControl} InGame={Game.InGame} MenuOpen={Game.MenuOpen} MapOpen={Game.MapOpen}");
            Key(Silk.NET.Input.Key.T);
        });
        Wait(15);
        Do((_, _) => Check(Inventory.GetHandCount(HandItem.Potion) == 1, "T na pausa não gasta item",
            $"potion={Inventory.GetHandCount(HandItem.Potion)}"));
        Tap(Button.Start);
        Wait(60);
        WaitIdle();
        Do((_, _) => Check(Inventory.GetHandCount(HandItem.Potion) == 1, "T da pausa não dispara ao voltar ao jogo",
            $"potion={Inventory.GetHandCount(HandItem.Potion)}"));
        // the pad path: L2 through the game's own pad buffer uses an item and keeps the character
        Do((_, m) => { Player.Hp = Player.HpMax - 20; _probeX = (int)m.ReadU32(Game.PlayableCharacterAddr); });
        Tap(Button.L2, 4);
        Wait(10);
        Do((_, m) => Check(Inventory.GetHandCount(HandItem.Potion) == 0 && Player.Hp == Player.HpMax &&
                           (int)m.ReadU32(Game.PlayableCharacterAddr) == _probeX,
            "L2 do controle usa a Potion sem trocar de personagem",
            $"potion={Inventory.GetHandCount(HandItem.Potion)} hp={Player.Hp}/{Player.HpMax} char {_probeX}->{m.ReadU32(Game.PlayableCharacterAddr)}"));
        Wait(40);
        Do((_, _) => { Player.Hp = Player.HpMax; _topUpHp = true; });

        // ---- minimap cells and the partner's position, from a transform built like NetworkManager's ----
        for (int pass = 0; pass < 2; pass++)
        {
            string tag = pass == 0 ? "parado" : "depois de andar";
            Do((_, m) =>
            {
                var local = BloodstainedMinimapHud.LocalCell(m);
                byte visited = Map.GetRoom(local.X, local.Y);
                Check(visited != 0, $"Minimapa ({tag}): célula do jogador está marcada no mapa do jogo",
                    $"cell={local} room={visited} visitadas={Map.VisitedCount} origem=({Stages.RoomX},{Stages.RoomY}) mundo=({WorldX(m)},{WorldY(m)})");

                byte[] payload = NetworkPacket.CreatePlayerTransform(
                    (byte)Stages.Current, (byte)Stages.RoomX, (byte)Stages.RoomY,
                    (short)WorldX(m), (short)WorldY(m), 0, 0, false, 0, 0,
                    (ushort)Player.Hp, (ushort)Player.HpMax, 0, 1, 0, 0);
                RemotePuppet.ReceiveTransform(payload);

                var partner = BloodstainedMinimapHud.PartnerCell();
                Check(partner == local, $"Minimapa ({tag}): parceiro no mesmo lugar cai na mesma célula",
                    $"parceiro={partner} local={local}");
                int oldX = m.ReadU8(0x800730B0) + RemotePuppet.RoomX;
                Console.WriteLine($"[HUDTEST] fórmula antiga daria X={oldX} (correto {local.X})");

                short scrollX = (short)m.ReadU16(0x8007308E);
                short scrollY = (short)m.ReadU16(0x80073092);
                float sx = HudLogic.WorldToScreen(RemotePuppet.PosX, scrollX);
                float sy = HudLogic.WorldToScreen(RemotePuppet.PosY, scrollY);
                Check(Math.Abs(sx - Player.PosX) <= 1 && Math.Abs(sy - Player.PosY) <= 1,
                    $"Puppet ({tag}): posição na tela coincide com a do jogador",
                    $"puppet=({sx},{sy}) jogador=({Player.PosX},{Player.PosY}) scroll=({scrollX},{scrollY})");
                RemotePuppet.Reset();
            });
            if (pass == 0)
            {
                Do((_, m) => { _mapBefore = new byte[Map.Length]; for (int i = 0; i < Map.Length; i++) _mapBefore[i] = m.ReadU8(Map.CastleMapAddr + (uint)i); });
                Press(Button.Right); Wait(90); Release(Button.Right); Wait(20);
                WaitIdle();
                Do((_, m) =>
                {
                    var c = BloodstainedMinimapHud.LocalCell(m);
                    for (int i = 0; i < Map.Length; i++)
                    {
                        byte now = m.ReadU8(Map.CastleMapAddr + (uint)i);
                        if (now != _mapBefore![i])
                            Console.WriteLine($"[HUDTEST] mapa mudou: byte {i} (0x{i:X3}) 0x{_mapBefore[i]:X2} -> 0x{now:X2} | célula agora={c} ; empacotado y*16+x/4={c.Y * 16 + c.X / 4} bit={(c.X & 3) * 2}");
                    }
                });
            }
        }

        // ---- Player Select: keys ----
        Do((_, _) =>
        {
            _snapshotMode = UltimateOptions.CharacterMode;
            PlayerSelectScreen.Open(1);
            Key(Silk.NET.Input.Key.Right);
            Check(PlayerSelectScreen.SelectedCard == 2 && PlayerSelectScreen.MariaRow == -1, "Seleção: Direita vai para a Maria com foco no card");
            Key(Silk.NET.Input.Key.Down);
            bool wasPsp = _snapshotMode == CharacterSelection.PSPMaria;
            Key(wasPsp ? Silk.NET.Input.Key.Left : Silk.NET.Input.Key.Right);
            var expect = wasPsp ? CharacterSelection.SaturnMaria : CharacterSelection.PSPMaria;
            Check(PlayerSelectScreen.MariaRow == 0 && UltimateOptions.CharacterMode == expect,
                "Seleção: Baixo + seta muda a Version da Maria", $"antes={_snapshotMode} agora={UltimateOptions.CharacterMode}");
        });
        HudShot("selecao_maria_config");
        Do((_, _) =>
        {
            Key(Silk.NET.Input.Key.Up);
            Key(Silk.NET.Input.Key.Left);
            Check(PlayerSelectScreen.SelectedCard == 1, "Seleção: Cima + Esquerda sai da Maria para o Alucard",
                $"card={PlayerSelectScreen.SelectedCard}");
            Key(Silk.NET.Input.Key.Left);
            Key(Silk.NET.Input.Key.Down);
            Key(Silk.NET.Input.Key.Down);
            Check(PlayerSelectScreen.SelectedCard == 0, "Seleção: Esquerda chega no Richter");
        });
        HudShot("selecao_richter_trajes");
        Do((_, _) =>
        {
            Key(Silk.NET.Input.Key.Escape);
            Check(!PlayerSelectScreen.IsOpen && UltimateOptions.CharacterMode == _snapshotMode,
                "Seleção: Esc fecha e desfaz a mudança de Version", $"modo={UltimateOptions.CharacterMode} antes={_snapshotMode}");
        });
        Wait(20);
        WaitIdle();

        // ---- Player Select: the game must not move while it is open ----
        Do((_, m) => { PlayerSelectScreen.Open(1); _probeX = WorldX(m); Console.WriteLine("[HUDTEST] KEYHOLD 0x25 1000"); });
        TraceInput("aberta", 200); // the capture script holds the real Left arrow on the window for 1 s
        Do((_, m) =>
        {
            // something outside the test (a key typed while the game had focus: P / F3 toggle the
            // screen) can close it; that is not the input block failing, so it is reported as such
            Check(_closedFrames == 0, "Seleção aberta: a tela ficou aberta durante todo o seguro da tecla",
                $"fechada em {_closedFrames} amostras (tecla externa?)");
            Check(_hwFrames > 0, "Seleção aberta: a tecla Esquerda real chegou ao controle", $"amostras={_hwFrames}");
            Check(_gameFrames == 0 && _walkFrames == 0,
                "Seleção aberta: o jogo não vê a Esquerda e o personagem não anda enquanto ela é segurada",
                $"jogo viu em {_gameFrames}/{_hwFrames} amostras, andando em {_walkFrames}, x {_xHoldStart} -> {_xHoldEnd}");
            Check(Player.Step != PlayerStep.Crouching, "Seleção aberta: o jogo não recebe todos os botões apertados", $"step={Player.Step}");
            PlayerSelectScreen.Cancel();
            _probeX = WorldX(m);
        });
        Wait(10);
        Do((_, _) => Console.WriteLine("[HUDTEST] KEYHOLD 0x25 1000"));
        TraceInput("fechada", 200);
        Do((_, m) => Check(_hwFrames > 0 && _gameFrames > 0 && _walkFrames > 0 && _xHoldEnd < _xHoldStart - 40,
            "Seleção fechada: a mesma tecla real volta a mover (controle do teste)",
            $"jogo viu em {_gameFrames}/{_hwFrames} amostras, andando em {_walkFrames}, x {_xHoldStart} -> {_xHoldEnd}"));
        Wait(20);

        Do((_, _) =>
        {
            Console.WriteLine($"[HUDTEST] RESULTADO: {_hudPass} passaram | {_hudFail} falharam");
            Console.WriteLine("[MariaTest] SEQUÊNCIA COMPLETA");
        });
        Wait(30);
        Do((_, _) => Environment.Exit(_hudFail == 0 ? 0 : 1));
    }

    private static CharacterSelection _snapshotMode;

    // x is only reported: an enemy periodically pushes the player (step 0x0A) with or without input
    private static int _hwFrames, _gameFrames, _walkFrames, _closedFrames, _xHoldStart, _xHoldEnd;

    private static void TraceInput(string tag, int frames)
    {
        for (int f = 0; f < frames; f += 10)
        {
            int at = f;
            Do((_, m) =>
            {
                if (at == 0) { _hwFrames = _gameFrames = _walkFrames = _closedFrames = 0; _xHoldStart = _xHoldEnd = int.MinValue; }
                bool hw = (RecompOne.Runtime.Hardware.Controller.State & RecompOne.Runtime.Hardware.Controller.Left) == 0;
                bool game = (m.ReadU16(Game.PadsAddr) & (ushort)Button.Left) != 0;
                if (hw)
                {
                    _hwFrames++;
                    if (_xHoldStart == int.MinValue) _xHoldStart = WorldX(m);
                    _xHoldEnd = WorldX(m);
                }
                if (hw && game) _gameFrames++;
                if (tag == "aberta" && !PlayerSelectScreen.IsOpen) _closedFrames++;
                if (hw && Player.Step == PlayerStep.Walking) _walkFrames++;
                Console.WriteLine(
                    $"[HUDTEST] trace {tag} f={at} x={WorldX(m)} step={(int)Player.Step:X2} card={PlayerSelectScreen.SelectedCard} aberta={PlayerSelectScreen.IsOpen} held=0x{Held:X4} " +
                    $"g_pads=0x{m.ReadU16(Game.PadsAddr):X4} hw=0x{RecompOne.Runtime.Hardware.Controller.State:X4} lx={RecompOne.Runtime.Hardware.Controller.LeftX}");
            });
            Wait(10);
        }
    }
    private static byte[]? _mapBefore;
}
