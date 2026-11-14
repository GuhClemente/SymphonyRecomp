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
    private static bool _isLoaded = false;
    public static bool IsLoaded => _isLoaded;
    private static bool _soundsLoaded = false;
    public static bool SoundsLoaded => _soundsLoaded;

    public static bool TransformToMaria(IMemory m)
    {
        try
        {
            string binPath = Path.GetFullPath("assets/characters/MARIA.BIN");
            if (!File.Exists(binPath))
            {
                Console.WriteLine($"[MariaLoader] Arquivo não encontrado: {binPath}");
                return false;
            }

            byte[] mariaBin = File.ReadAllBytes(binPath);
            Console.WriteLine($"[MariaLoader] Carregando {mariaBin.Length} bytes de MARIA.BIN em 0x8013C000...");

            // 1. Grava os bytes de dados (sprites LZSS, tabelas de animação e paletas) na RAM do PS1
            for (int i = 0; i < mariaBin.Length; i++)
            {
                m.WriteU8(0x8013C000u + (uint)i, mariaBin[i]);
            }

            // 2. Carrega a tabela de dispatch remapeada para a Maria no Dispatcher
            Dispatcher.Load("maria");

            // 3. Define a flag de personagem para Maria (2)
            m.WriteU32(Game.PlayableCharacterAddr, (uint)PlayableCharacter.Maria);

            // 4. Executa a rotina de inicialização MarInit (func_80161DA8)
            var ctx = new CpuContext
            {
                SP = 0x801FFF00u, // stack válida
                A0 = 0
            };
            SoTN_maria.func_80161DA8(ctx, m);

            // 5. Carrega os bancos de áudio autênticos (MARIA_SD.BIN) na SPU e tabelas do jogo
            LoadMariaSounds(m);

            _isLoaded = true;
            Console.WriteLine("[MariaLoader] Maria Renard carregada e inicializada com sucesso!");
            ToastNotifications.ShowText(
                "🌟 Maria Renard (Saturn)",
                "Transformação concluída! Sprites, paletas, rotinas e áudio ativados.",
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

    public static bool LoadMariaSounds(IMemory m)
    {
        try
        {
            string sdPath = Path.GetFullPath("assets/characters/MARIA_SD.BIN");
            if (!File.Exists(sdPath))
            {
                Console.WriteLine($"[MariaLoader] Arquivo de áudio não encontrado: {sdPath}");
                return false;
            }

            byte[] sd = File.ReadAllBytes(sdPath);
            Console.WriteLine($"[MariaLoader] Carregando bancos de som da Maria ({sd.Length} bytes de MARIA_SD.BIN)...");

            // 1. Grava os cabeçalhos VAB na RAM do PS1
            // SD_RIH.VH: offset 0x0, tamanho 0x1800 (3 setores = 6144 bytes) -> 0x800B407C
            int vh1Len = Math.Min(0x1800, sd.Length);
            for (int i = 0; i < vh1Len; i++)
            {
                m.WriteU8(0x800B407Cu + (uint)i, sd[i]);
            }

            // SD_MAR.VH: offset 0x10000, tamanho 0x1000 (2 setores = 4096 bytes) -> 0x800B807C
            if (sd.Length >= 0x10000)
            {
                int vh2Len = Math.Min(0x1000, sd.Length - 0x10000);
                for (int i = 0; i < vh2Len; i++)
                {
                    m.WriteU8(0x800B807Cu + (uint)i, sd[0x10000 + i]);
                }
            }

            // 2. Transfere os samples de som ADPCM diretamente para a SPU RAM
            var spu = RecompOne.Runtime.Runtime.Spu;
            if (spu != null)
            {
                // VAB 1 Body (SD_RIH.VB): offset 0x1800, tamanho 0xE190 -> SPU RAM 0x42CC0
                if (sd.Length >= 0x1800)
                {
                    int vb1Len = Math.Min(0xE190, sd.Length - 0x1800);
                    Array.Copy(sd, 0x1800, spu.Ram, 0x42CC0, vb1Len);
                }

                // VAB 2 Body (SD_MAR.VB): offset 0x11000, tamanho 0xFBF0 -> SPU RAM 0x50E50
                if (sd.Length >= 0x11000)
                {
                    int vb2Len = Math.Min(0xFBF0, sd.Length - 0x11000);
                    Array.Copy(sd, 0x11000, spu.Ram, 0x50E50, vb2Len);
                }
                Console.WriteLine("[MariaLoader] SPU RAM populada com samples de áudio da Maria (0x42CC0 e 0x50E50).");
            }

            // 3. Atualiza os registros do driver de som da PsyQ
            var ctx = new CpuContext { SP = 0x801FFF00u };

            // Fechar e reabrir VAB 1 (Voz e ataques)
            ctx.A0 = 1u;
            SoTN_main.SsVabClose(ctx, m);

            ctx.A0 = 0x800B407Cu;
            ctx.A1 = 1u;
            ctx.A2 = 0x42CC0u; // Endereço de SPU RAM para VAB 1
            SoTN_main.SsVabOpenHeadSticky(ctx, m);

            // Fechar e reabrir VAB 2 (Especiais e invocações)
            ctx.A0 = 2u;
            SoTN_main.SsVabClose(ctx, m);

            ctx.A0 = 0x800B807Cu;
            ctx.A1 = 2u;
            ctx.A2 = 0x50E50u; // Endereço de SPU RAM para VAB 2
            SoTN_main.SsVabOpenHeadSticky(ctx, m);

            // 4. Aplica a tabela de prioridades da Maria (g_ModMarSfxPrio) em g_SfxData
            const uint sfxDataBase = 0x800BF554u;
            const ushort sfxStart = 0x600;

            (ushort id, byte prio)[] marSfxPrio =
            [
                (0x6F9, 90), (0x6FA, 90), (0x6FB, 90), // grunts
                (0x6FC, 90), (0x702, 90), (0x703, 90), (0x704, 90), // kick, hurt
                (0x678, 65), (0x62D, 65), (0x62E, 65), // hits
                (0x6FD, 60), (0x6FE, 60), (0x700, 60), // shot, fire, ball
                (0x6F4, 60), (0x6F5, 60), (0x6F7, 60), // lunge, beasts end, flap
                (0x887, 70), (0x888, 70), (0x889, 70), (0x88A, 70), // Genbu, Byakko, Seiryuu, Dragon roar
                (0x707, 85), (0x617, 85) // slide skid, scrape
            ];

            foreach (var (id, prio) in marSfxPrio)
            {
                uint entry = sfxDataBase + (uint)(id - sfxStart) * 7;
                m.WriteU8(entry + 6, prio);
            }
            // Modo de canais do Seiryuu (canais 22 e 23)
            m.WriteU8(sfxDataBase + (uint)(0x889 - sfxStart) * 7 + 0, 9);

            _soundsLoaded = true;
            Console.WriteLine("[MariaLoader] Bancos de som e prioridades da Maria ativados com sucesso!");
            return true;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[MariaLoader] Erro ao carregar sons da Maria: {ex}");
            return false;
        }
    }
}
