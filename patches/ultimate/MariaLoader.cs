using System;
using System.IO;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Dispatch;
using RecompOne.Runtime.Host.Window;
using RecompOne.Runtime.Memory;
using Sotn;

namespace Recompiled.Ultimate;

public static class MariaLoader
{
    public const string OverlayName = "maria";

    // The player overlay slot (RIC.BIN in retail). Stage overlays start at 0x80180000.
    private const uint PlayerOverlayBase = 0x8013C000u;
    private const uint PlayerOverlayLimit = 0x80180000u;
    private const uint MarInitAddr = 0x80161DA8u; // MarInit (RicInit in RIC.BIN)

    // VAB header buffers inside DRA and the sound RAM windows DRA transfers their bodies to
    // (mods/sotn-ultimate/tools/gen_mar_sounds.py)
    private const uint Vab1HeaderAddr = 0x800B407Cu;
    private const uint Vab2HeaderAddr = 0x800B807Cu;
    private const int Vab1SpuAddr = 0x42CC0;
    private const int Vab2SpuAddr = 0x50E50;
    private const int Vab1BodyMax = 0xE190;
    private const int Vab2BodyMax = 0xFBF0;

    // g_SfxData: 7 bytes per sound {vabid, prog, note, volume, mode, tone, priority}
    private const uint SfxDataBase = 0x800BF554u;
    private const int SfxStart = 0x600;
    private const uint SfxModeOff = 4;
    private const uint SfxPrioOff = 6;
    private const byte SfxModeChannels12_19 = 0;
    private const byte SfxModeChannels22_23 = 1;

    // g_ModMarSfxPrio (mods/sotn-ultimate/src/disc/cd_mod_files.h): id, retail, Saturn Maria
    private static readonly (ushort Id, byte Retail, byte Maria)[] SfxPrio =
    [
        (0x6F9, 80, 90), (0x6FA, 80, 90), (0x6FB, 80, 90), // grunts
        (0x6FC, 80, 90), (0x702, 81, 90),                  // kick, hurt
        (0x703, 81, 90), (0x704, 81, 90),                  // hurt
        (0x678, 40, 65), (0x62D, 30, 65), (0x62E, 32, 65), // her hits
        (0x6FD, 80, 60), (0x6FE, 80, 60), (0x700, 80, 60), // shot, fire, ball
        (0x6F4, 80, 60), (0x6F5, 80, 60),                  // lunge, beasts end
        (0x6F7, 50, 60), (0x887, 90, 70), (0x888, 90, 70), // flap, Genbu, Byakko
        (0x889, 90, 70), (0x88A, 90, 70),                  // Seiryuu, its roar
        (0x707, 40, 85), (0x617, 30, 85),                  // slide skid, scrape
    ];
    private const ushort SeiryuuSfx = 0x889;

    private static bool _soundsLoaded;
    private static bool _sfxPatched;

    public static bool OverlayAvailable => Dispatcher.Overlays.ContainsKey(OverlayName);

    /// <summary>True while the running session is actually playing as Maria.</summary>
    public static bool IsActive => Game.Available && Player.IsMaria;

    public static bool SoundsLoaded => _soundsLoaded;

    /// <summary>Safe from any thread: the transformation runs on the game thread next frame.</summary>
    public static void RequestTransform() => GameThread.Post((c, m) =>
    {
        if (Game.InGame && !Game.IsLoading) TransformToMaria(c, m);
    });

    /// <summary>Safe from any thread.</summary>
    public static void RequestReloadSounds() => GameThread.Post((c, m) => LoadMariaSounds(c, m));

    private static string? ResolveAsset(string name)
    {
        string relative = Path.Combine("assets", "characters", name);
        foreach (string root in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
        {
            string path = Path.Combine(root, relative);
            if (File.Exists(path)) return path;
        }
        return null;
    }

    // Runs a recompiled function on the live game context and puts every register back afterwards.
    // The stack pointer is moved below the interrupted frame so the callee cannot touch it.
    private static uint Invoke(CpuContext c, IMemory m, Action<CpuContext, IMemory> fn,
        uint a0 = 0, uint a1 = 0, uint a2 = 0)
    {
        var snap = c.Snapshot();
        try
        {
            c.SP -= 0x40u;
            c.A0 = a0;
            c.A1 = a1;
            c.A2 = a2;
            fn(c, m);
            return c.V0;
        }
        finally
        {
            c.Restore(snap);
        }
    }

    /// <summary>Must be called on the game thread (see <see cref="RequestTransform"/>).</summary>
    public static bool TransformToMaria(CpuContext c, IMemory m)
    {
        try
        {
            if (!OverlayAvailable)
            {
                Console.WriteLine("[MariaLoader] Overlay 'maria' não foi gerado. Rode o build inicial novamente (config/sotn.json -> overlay 'maria').");
                ToastNotifications.ShowText("Maria Renard", "Overlay da Maria ausente: refaça o build inicial.", null, 5f);
                return false;
            }

            string? binPath = ResolveAsset("MARIA.BIN");
            if (binPath == null)
            {
                Console.WriteLine("[MariaLoader] Arquivo não encontrado: assets/characters/MARIA.BIN");
                ToastNotifications.ShowText("Maria Renard", "assets/characters/MARIA.BIN não encontrado.", null, 5f);
                return false;
            }

            byte[] mariaBin = File.ReadAllBytes(binPath);
            if (mariaBin.Length > PlayerOverlayLimit - PlayerOverlayBase)
            {
                Console.WriteLine($"[MariaLoader] MARIA.BIN grande demais ({mariaBin.Length} bytes) para o slot do personagem.");
                return false;
            }

            Console.WriteLine($"[MariaLoader] Carregando {mariaBin.Length} bytes de MARIA.BIN em 0x{PlayerOverlayBase:X8}...");

            // 1. Dados do overlay (sprites, tabelas de animação, paletas) no slot do personagem
            for (int i = 0; i < mariaBin.Length; i++)
                m.WriteU8(PlayerOverlayBase + (uint)i, mariaBin[i]);

            // 2. Código recompilado da Maria no lugar do RIC
            Dispatcher.Load(OverlayName);

            // 3. Personagem jogável = Maria
            m.WriteU32(Game.PlayableCharacterAddr, (uint)PlayableCharacter.Maria);

            // 4. MarInit
            Invoke(c, m, (cc, mm) => Dispatcher.Call(cc, mm, MarInitAddr));

            // 5. Bancos de áudio (MARIA_SD.BIN)
            bool sounds = LoadMariaSounds(c, m);

            Console.WriteLine("[MariaLoader] Maria Renard carregada e inicializada.");
            ToastNotifications.ShowText(
                "🌟 Maria Renard (Saturn)",
                sounds
                    ? "Transformação concluída! Sprites, paletas, rotinas e áudio ativados."
                    : "Transformação concluída, mas os sons da Maria não puderam ser carregados.",
                null,
                4f);
            return true;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MariaLoader] Erro ao carregar Maria: {ex}");
            return false;
        }
    }

    /// <summary>Must be called on the game thread (see <see cref="RequestReloadSounds"/>).</summary>
    public static bool LoadMariaSounds(CpuContext c, IMemory m)
    {
        try
        {
            string? sdPath = ResolveAsset("MARIA_SD.BIN");
            if (sdPath == null)
            {
                Console.WriteLine("[MariaLoader] Arquivo de áudio não encontrado: assets/characters/MARIA_SD.BIN");
                return false;
            }

            var spu = RecompOne.Runtime.Runtime.Spu;
            if (spu == null)
            {
                Console.WriteLine("[MariaLoader] SPU indisponível, sons da Maria não carregados.");
                return false;
            }

            byte[] sd = File.ReadAllBytes(sdPath);
            // SD_RIH.VH @0 (3 setores), SD_RIH.VB @0x1800, SD_MAR.VH @0x10000 (2 setores), SD_MAR.VB @0x11000
            if (sd.Length < 0x11000)
            {
                Console.WriteLine($"[MariaLoader] MARIA_SD.BIN inválido ({sd.Length} bytes).");
                return false;
            }

            Console.WriteLine($"[MariaLoader] Carregando bancos de som da Maria ({sd.Length} bytes)...");

            bool ok = OpenVab(c, m, spu.Ram, sd, 1, 0x0, 0x1800, Vab1HeaderAddr, Vab1SpuAddr, Vab1BodyMax)
                      & OpenVab(c, m, spu.Ram, sd, 2, 0x10000, 0x1000, Vab2HeaderAddr, Vab2SpuAddr, Vab2BodyMax);

            ApplySfxPriorities(m, maria: true);
            _sfxPatched = true;

            _soundsLoaded = ok;
            Console.WriteLine(ok
                ? "[MariaLoader] Bancos de som e prioridades da Maria ativados."
                : "[MariaLoader] Falha ao abrir os bancos de som da Maria.");
            return ok;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MariaLoader] Erro ao carregar sons da Maria: {ex}");
            return false;
        }
    }

    private static bool OpenVab(CpuContext c, IMemory m, byte[] spuRam, byte[] sd, uint vabId,
        int headerOff, int headerLen, uint headerAddr, int spuAddr, int bodyMax)
    {
        int bodyOff = headerOff + headerLen;
        int bodyLen = Math.Min(bodyMax, (vabId == 1 ? 0x10000 : sd.Length) - bodyOff);
        if (bodyLen <= 0 || spuAddr + bodyLen > spuRam.Length) return false;

        // Cabeçalho VAB no buffer do DRA
        for (int i = 0; i < headerLen; i++)
            m.WriteU8(headerAddr + (uint)i, sd[headerOff + i]);

        // Samples ADPCM direto na sound RAM
        Array.Copy(sd, bodyOff, spuRam, spuAddr, bodyLen);

        Invoke(c, m, SoTN_main.SsVabClose, vabId);
        int opened = (int)Invoke(c, m, SoTN_main.SsVabOpenHeadSticky, headerAddr, vabId, (uint)spuAddr);
        if (opened < 0)
        {
            Console.WriteLine($"[MariaLoader] SsVabOpenHeadSticky falhou para o VAB {vabId}.");
            return false;
        }

        // The body is already in sound RAM: without this the VAB stays "head only" (2) and
        // SpuVmVSetUp refuses every key-on for it, which mutes all of her sounds.
        int ready = (int)Invoke(c, m, SoTN_main.SsVabFakeBody, vabId);
        if (ready < 0)
        {
            Console.WriteLine($"[MariaLoader] SsVabFakeBody falhou para o VAB {vabId}.");
            return false;
        }

        return true;
    }

    private static void ApplySfxPriorities(IMemory m, bool maria)
    {
        foreach (var (id, retail, mar) in SfxPrio)
            m.WriteU8(SfxEntry(id) + SfxPrioOff, maria ? mar : retail);

        // Seiryuu is a loop for her: channels 22/23, which her dragon releases
        m.WriteU8(SfxEntry(SeiryuuSfx) + SfxModeOff, maria ? SfxModeChannels22_23 : SfxModeChannels12_19);
    }

    private static uint SfxEntry(ushort id) => SfxDataBase + (uint)(id - SfxStart) * 7;

    /// <summary>
    /// Called when the session stops playing as Maria. DRA keeps g_SfxData for the whole session, so
    /// the retail priorities have to be put back; the game reloads Richter's own VABs with RIC.BIN.
    /// </summary>
    public static void OnMariaLeft(IMemory m)
    {
        _soundsLoaded = false;
        if (!_sfxPatched) return;
        _sfxPatched = false;
        try
        {
            ApplySfxPriorities(m, maria: false);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MariaLoader] Erro ao restaurar prioridades de SFX: {ex.Message}");
        }
    }
}
