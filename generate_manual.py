#!/usr/bin/env python3
"""
SymphonyRecomp - Gerador do Manual Oficial Definitivo em PDF e HTML Interativo
Edição Master: Capturas reais de gameplay em resolução nativa widescreen 1280x720,
cobrindo cada ação (ataques, magias, pulos, item crashes, subweapons e HUDs autênticos),
comparativo Windows vs Linux WSL, guia de Multiplayer Co-op & PvP, Fast Travel e créditos completos.

Para regerar este manual a qualquer momento:
    python generate_manual.py
"""

import os
import sys
import base64
import subprocess
from pathlib import Path
from PIL import Image

ROOT_DIR = Path(__file__).resolve().parent
DOCS_DIR = ROOT_DIR / "docs"
MANUAL_DIR = DOCS_DIR / "manual"
IMG_DIR = MANUAL_DIR / "img"
OUTPUT_HTML = MANUAL_DIR / "manual_symphonyrecomp.html"
OUTPUT_PDF = ROOT_DIR / "SymphonyRecomp_Manual_Definitivo.pdf"
DOCS_PDF = MANUAL_DIR / "SymphonyRecomp_Manual_Definitivo.pdf"

MANUAL_DIR.mkdir(parents=True, exist_ok=True)
IMG_DIR.mkdir(parents=True, exist_ok=True)

def image_to_base64(path_obj: Path) -> str:
    """Converte arquivo de imagem para base64 data-uri."""
    if not path_obj.exists():
        return ""
    suffix = path_obj.suffix.lower()
    mime = "image/png" if suffix == ".png" else "image/jpeg"
    with open(path_obj, "rb") as f:
        data = base64.b64encode(f.read()).decode("utf-8")
    return f"data:{mime};base64,{data}"

def find_chromium_browser():
    """Localiza o Microsoft Edge ou Google Chrome no Windows/macOS/Linux."""
    candidates = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
        "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "google-chrome",
        "microsoft-edge",
        "chromium-browser",
        "chromium",
    ]
    for c in candidates:
        if os.path.isabs(c):
            if os.path.isfile(c) and os.access(c, os.X_OK):
                return c
        else:
            from shutil import which
            path = which(c)
            if path:
                return path
    return None

def build_html():
    print("   Carregando e codificando capturas reais de gameplay em 1280x720...")

    # Imagens Maria Saturn
    img_ms_idle = image_to_base64(IMG_DIR / "001_maria_saturn_parado.png")
    img_ms_atk = image_to_base64(IMG_DIR / "002_maria_saturn_ataque.png")
    img_ms_jump = image_to_base64(IMG_DIR / "010_maria_saturn_pulo_triplo.png")
    img_ms_charge = image_to_base64(IMG_DIR / "025_maria_saturn_carga_3.png")
    img_ms_suzaku = image_to_base64(IMG_DIR / "013_maria_saturn_especial_2_suzaku.png")
    img_ms_seiryuu = image_to_base64(IMG_DIR / "019_maria_saturn_especial_5_seiryuu.png")
    img_ms_knuckle = image_to_base64(IMG_DIR / "011_maria_saturn_especial_1_soco_guardiao.png")
    img_ms_genbu = image_to_base64(IMG_DIR / "015_maria_saturn_especial_3_genbu.png")
    img_ms_byakko = image_to_base64(IMG_DIR / "017_maria_saturn_especial_4_byakko.png")
    img_ms_anthem = image_to_base64(IMG_DIR / "021_maria_saturn_especial_6_quatro_bestas.png")

    # Imagens Maria PSP
    img_mp_idle = image_to_base64(IMG_DIR / "027_maria_psp_parado.png")
    img_mp_hud = image_to_base64(IMG_DIR / "036_maria_psp_icones.png")
    img_mp_atk = image_to_base64(IMG_DIR / "028_maria_psp_ataque.png")
    img_mp_spec = image_to_base64(IMG_DIR / "040_maria_psp_triangulo.png")
    img_mp_swap = image_to_base64(IMG_DIR / "038_maria_psp_icones_trocados.png")

    # Imagens Richter Belmont Clássico
    img_rc_idle = image_to_base64(IMG_DIR / "043_richter_parado.png")
    img_rc_whip = image_to_base64(IMG_DIR / "044_richter_ataque.png")
    img_rc_hydro = image_to_base64(IMG_DIR / "052_richter_crash_1_hydro_storm.png")
    img_rc_grand = image_to_base64(IMG_DIR / "054_richter_crash_2_grand_cross.png")
    img_rc_blades = image_to_base64(IMG_DIR / "056_richter_crash_3_thousand_blades.png")
    img_rc_dash = image_to_base64(IMG_DIR / "062_richter_blade_dash.png")

    # Imagens Richter Saturn Coat
    img_rcoat_idle = image_to_base64(IMG_DIR / "064_richter_casaco_parado.png")
    img_rcoat_whip = image_to_base64(IMG_DIR / "065_richter_casaco_ataque.png")
    img_rcoat_hydro = image_to_base64(IMG_DIR / "073_richter_casaco_crash_1_hydro_storm.png")
    img_rcoat_grand = image_to_base64(IMG_DIR / "075_richter_casaco_crash_2_grand_cross.png")

    # Imagens Richter Sobretudo Couro Marrom (Estilo Simon Belmont)
    img_rbrown_idle = image_to_base64(IMG_DIR / "028_richter_couro_marrom.png")
    img_rbrown_gameplay = image_to_base64(IMG_DIR / "029_gameplay_richter_couro_marrom.jpg")

    # Imagens Alucard
    img_al_idle = image_to_base64(IMG_DIR / "085_alucard_parado.png")
    img_al_atk = image_to_base64(IMG_DIR / "086_alucard_ataque.png")
    img_al_summon = image_to_base64(IMG_DIR / "092_alucard_magia_1_summon_spirit.png")
    img_al_tetra = image_to_base64(IMG_DIR / "094_alucard_magia_2_tetra_spirit.png")
    img_al_dark = image_to_base64(IMG_DIR / "096_alucard_magia_3_dark_metamorphosis.png")
    img_al_soul = image_to_base64(IMG_DIR / "100_alucard_magia_5_soul_steal.png")

    # Imagens Linux WSL
    img_lx_maria = image_to_base64(IMG_DIR / "linux_001_maria_saturn_parado.png")
    img_lx_psp = image_to_base64(IMG_DIR / "linux_003_maria_psp_parado.png")
    img_lx_richter = image_to_base64(IMG_DIR / "linux_005_richter_parado.png")
    img_lx_coat = image_to_base64(IMG_DIR / "linux_007_richter_casaco_parado.png")
    img_lx_alucard = image_to_base64(IMG_DIR / "linux_009_alucard_parado.png")

    # Mapas de Teleporte & Contato
    img_teleport_destinos = image_to_base64(ROOT_DIR / "docs/teste-teleporte/28_destinos.jpg")
    img_teleport_locais = image_to_base64(ROOT_DIR / "docs/teste-teleporte/10_locais_5_personagens_a.jpg")

    html = f"""<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SymphonyRecomp - Manual Definitivo do Jogador & Guia Completo</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Cinzel:wght@600;700;900&family=Cinzel+Decorative:wght@700&family=Inter:wght@300;400;500;600;700&display=swap');

@page {{
    size: A4 portrait;
    margin: 12mm 12mm 14mm 12mm;
    @bottom-right {{
        content: counter(page);
        font-family: 'Cinzel', serif;
        font-size: 10pt;
        color: #d4af37;
    }}
}}

* {{
    box-sizing: border-box;
    margin: 0;
    padding: 0;
}}

body {{
    background-color: #08070d;
    color: #ede9df;
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    line-height: 1.6;
    font-size: 14.5px;
}}

.manual-wrapper {{
    max-width: 1140px;
    margin: 0 auto;
    padding: 24px;
}}

h1, h2, h3, h4 {{
    font-family: 'Cinzel', serif;
    letter-spacing: 0.5px;
}}

/* =========================================================
   CAPA DO MANUAL
========================================================= */
.cover-page {{
    min-height: 94vh;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    text-align: center;
    background: radial-gradient(circle at center, #26101d 0%, #0d0713 70%, #050308 100%);
    border: 3px double #d4af37;
    padding: 45px 30px;
    position: relative;
    border-radius: 8px;
    box-shadow: 0 0 50px rgba(0, 0, 0, 0.9), inset 0 0 80px rgba(142, 21, 38, 0.35);
    margin-bottom: 40px;
}}

.cover-page::before {{
    content: "";
    position: absolute;
    top: 14px; left: 14px; right: 14px; bottom: 14px;
    border: 1px solid rgba(212, 175, 55, 0.45);
    pointer-events: none;
    border-radius: 4px;
}}

.cover-badge {{
    display: inline-block;
    padding: 8px 24px;
    border: 2px solid #d4af37;
    background: rgba(212, 175, 55, 0.15);
    color: #fce899;
    font-size: 13px;
    text-transform: uppercase;
    font-weight: 700;
    letter-spacing: 3px;
    border-radius: 25px;
    margin-bottom: 25px;
}}

.cover-title-container h1 {{
    font-family: 'Cinzel Decorative', 'Cinzel', serif;
    font-size: 46px;
    font-weight: 900;
    color: #fbf0c8;
    text-shadow: 0 0 25px rgba(220, 20, 60, 0.9), 0 0 50px rgba(212, 175, 55, 0.6);
    margin-bottom: 12px;
    line-height: 1.15;
}}

.cover-subtitle {{
    font-family: 'Cinzel', serif;
    font-size: 22px;
    color: #ff4d5a;
    letter-spacing: 5px;
    font-weight: 700;
    text-transform: uppercase;
    margin-bottom: 18px;
}}

.cover-tagline {{
    font-size: 17px;
    color: #c4bece;
    max-width: 820px;
    margin: 0 auto 35px;
    line-height: 1.6;
}}

.cover-features-grid {{
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 20px;
    width: 100%;
    margin-top: 25px;
}}

.cover-feat-box {{
    background: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(212, 175, 55, 0.4);
    padding: 22px 18px;
    border-radius: 8px;
}}

.cover-feat-box h4 {{
    color: #fbf0c8;
    font-size: 17px;
    margin-bottom: 8px;
}}

.cover-feat-box p {{
    font-size: 13.5px;
    color: #aba6ba;
    line-height: 1.5;
}}

.cover-footer {{
    margin-top: 35px;
    font-size: 13px;
    color: #8c889a;
    border-top: 1px solid rgba(212, 175, 55, 0.3);
    padding-top: 20px;
    width: 100%;
}}

/* =========================================================
   SEÇÕES & TÍTULOS
========================================================= */
.section-title {{
    color: #fce899;
    font-size: 26px;
    font-weight: 700;
    border-bottom: 3px solid #8e1526;
    padding-bottom: 10px;
    margin-top: 45px;
    margin-bottom: 22px;
    display: flex;
    align-items: center;
    gap: 12px;
}}

.section-title::before {{
    content: "🦇";
    font-size: 24px;
}}

.section-subtitle {{
    color: #ff5260;
    font-size: 19px;
    font-weight: 600;
    margin-top: 25px;
    margin-bottom: 14px;
}}

/* =========================================================
   CARTOES & BOXES
========================================================= */
.card {{
    background: rgba(26, 22, 38, 0.85);
    border: 1px solid rgba(212, 175, 55, 0.3);
    border-left: 6px solid #d4af37;
    border-radius: 8px;
    padding: 20px 24px;
    margin-bottom: 22px;
    font-size: 15.5px;
}}

.card-red {{
    border-left-color: #ff3847;
    background: rgba(42, 14, 24, 0.6);
}}

.card-purple {{
    border-left-color: #a855f7;
    background: rgba(35, 18, 48, 0.6);
}}

.card-gold {{
    border-left-color: #d4af37;
    background: rgba(45, 36, 20, 0.6);
}}

.card-emerald {{
    border-left-color: #10b981;
    background: rgba(16, 45, 35, 0.6);
}}

.card-title {{
    color: #fbf0c8;
    font-family: 'Cinzel', serif;
    font-size: 19px;
    font-weight: 700;
    margin-bottom: 8px;
}}

/* =========================================================
   TECLAS E BOTÕES
========================================================= */
kbd {{
    display: inline-block;
    padding: 3px 9px;
    font-family: 'Inter', monospace;
    font-size: 13.5px;
    font-weight: 700;
    line-height: 1.3;
    color: #ffffff;
    background: linear-gradient(180deg, #443c5b 0%, #282436 100%);
    border: 1px solid #736994;
    border-bottom: 3px solid #14121c;
    border-radius: 5px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.5);
    margin: 0 2px;
}}

.pad-btn {{
    display: inline-block;
    padding: 3px 10px;
    font-family: 'Inter', sans-serif;
    font-size: 13.5px;
    font-weight: 700;
    color: #fce899;
    background: rgba(212, 175, 55, 0.2);
    border: 1px solid rgba(212, 175, 55, 0.55);
    border-radius: 14px;
    margin: 0 3px;
}}

/* =========================================================
   TABELAS
========================================================= */
table {{
    width: 100%;
    border-collapse: collapse;
    margin: 18px 0 28px 0;
    font-size: 14.5px;
    background: rgba(18, 15, 27, 0.7);
    border-radius: 8px;
    overflow: hidden;
    border: 1px solid rgba(212, 175, 55, 0.25);
}}

th {{
    background: #2a1221;
    color: #fce899;
    font-family: 'Cinzel', serif;
    font-weight: 700;
    text-align: left;
    padding: 14px 16px;
    border-bottom: 2px solid #d4af37;
    text-transform: uppercase;
    font-size: 14px;
    letter-spacing: 1px;
}}

td {{
    padding: 12px 16px;
    border-bottom: 1px solid rgba(255, 255, 255, 0.08);
    vertical-align: middle;
}}

tr:nth-child(even) {{
    background: rgba(255, 255, 255, 0.03);
}}

tr:hover {{
    background: rgba(212, 175, 55, 0.06);
}}

/* =========================================================
   PERSONAGENS & GALERIAS DE CAPTURA EM 1280x720
========================================================= */
.char-card {{
    background: rgba(22, 18, 32, 0.9);
    border: 2px solid rgba(212, 175, 55, 0.35);
    border-radius: 10px;
    padding: 24px;
    margin-bottom: 35px;
    box-shadow: 0 8px 24px rgba(0,0,0,0.6);
}}

.char-header {{
    display: flex;
    align-items: center;
    justify-content: space-between;
    border-bottom: 2px solid rgba(212, 175, 55, 0.3);
    padding-bottom: 10px;
    margin-bottom: 16px;
}}

.char-name {{
    font-size: 24px;
    color: #fce899;
    font-weight: 700;
}}

.char-badge {{
    display: inline-block;
    padding: 5px 14px;
    background: #7a1524;
    color: #fff;
    font-size: 12px;
    border-radius: 15px;
    font-weight: bold;
    letter-spacing: 1px;
    text-transform: uppercase;
}}

.badge-psp {{ background: #1d3557; }}
.badge-coat {{ background: #441785; }}
.badge-alu {{ background: #5c111c; }}
.badge-lx {{ background: #0f5132; }}

.char-desc {{
    font-size: 16px;
    margin-bottom: 14px;
    color: #ded8e8;
    line-height: 1.6;
}}

.bullet-list {{
    list-style-type: none;
    padding-left: 0;
    margin-bottom: 20px;
}}

.bullet-list li {{
    position: relative;
    padding-left: 24px;
    margin-bottom: 10px;
    font-size: 15px;
}}

.bullet-list li::before {{
    content: "◆";
    position: absolute;
    left: 4px;
    color: #d4af37;
    font-size: 13px;
}}

.highlight {{
    color: #ff5260;
    font-weight: bold;
}}

/* =========================================================
   GRID DE CAPTURAS REAIS DE GAMEPLAY (1280x720)
========================================================= */
.gameplay-grid {{
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
    gap: 18px;
    margin: 22px 0;
}}

.gameplay-card {{
    background: #000;
    border: 2px solid rgba(212, 175, 55, 0.5);
    border-radius: 8px;
    padding: 8px;
    box-shadow: 0 4px 15px rgba(0,0,0,0.7);
    transition: transform 0.2s, box-shadow 0.2s, border-color 0.2s;
    display: flex;
    flex-direction: column;
}}

.gameplay-card:hover {{
    transform: translateY(-4px);
    border-color: #fce899;
    box-shadow: 0 8px 25px rgba(212, 175, 55, 0.4);
}}

.gameplay-card img {{
    width: 100%;
    aspect-ratio: 16 / 9;
    object-fit: cover;
    border-radius: 4px;
    display: block;
    cursor: zoom-in;
    background: #111;
}}

.gameplay-label {{
    font-size: 14px;
    color: #fce899;
    font-family: 'Cinzel', serif;
    font-weight: 700;
    margin-top: 10px;
    display: block;
}}

.gameplay-sublabel {{
    font-size: 12.5px;
    color: #aba6ba;
    margin-top: 3px;
    line-height: 1.4;
    display: block;
}}

/* =========================================================
   MODAL LIGHTBOX INTERATIVO
========================================================= */
.modal {{
    display: none;
    position: fixed;
    z-index: 99999;
    left: 0;
    top: 0;
    width: 100%;
    height: 100%;
    background-color: rgba(5, 3, 10, 0.94);
    backdrop-filter: blur(8px);
    justify-content: center;
    align-items: center;
    flex-direction: column;
    padding: 20px;
}}

.modal.active {{
    display: flex;
}}

.modal-content {{
    max-width: 95vw;
    max-height: 85vh;
    border: 3px solid #d4af37;
    border-radius: 8px;
    box-shadow: 0 0 50px rgba(212, 175, 55, 0.6);
    animation: zoom 0.25s ease-out;
}}

.modal-caption {{
    margin-top: 15px;
    color: #fce899;
    font-family: 'Cinzel', serif;
    font-size: 17px;
    text-align: center;
    max-width: 850px;
    line-height: 1.5;
}}

.modal-close {{
    position: absolute;
    top: 25px;
    right: 35px;
    color: #fce899;
    font-size: 42px;
    font-weight: bold;
    cursor: pointer;
    transition: 0.2s;
}}

.modal-close:hover {{
    color: #ff3847;
    transform: scale(1.15);
}}

@keyframes zoom {{
    from {{ transform: scale(0.85); opacity: 0; }}
    to {{ transform: scale(1); opacity: 1; }}
}}

.page-break {{
    page-break-after: always;
    margin-bottom: 30px;
}}

.no-break {{
    page-break-inside: avoid;
}}

.credits-grid {{
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 20px;
    margin-top: 20px;
}}

.credits-block {{
    background: rgba(25, 20, 35, 0.85);
    border: 1px solid rgba(212, 175, 55, 0.3);
    border-radius: 8px;
    padding: 18px;
}}

.credits-block h4 {{
    color: #fce899;
    font-size: 16px;
    border-bottom: 1px solid rgba(212, 175, 55, 0.3);
    padding-bottom: 6px;
    margin-bottom: 10px;
}}

.credits-block ul {{
    list-style: none;
    padding-left: 0;
}}

.credits-block li {{
    font-size: 13.5px;
    color: #c4bece;
    margin-bottom: 8px;
    line-height: 1.4;
}}

.chapter-footer {{
    text-align: center;
    font-size: 12px;
    color: #7b768a;
    margin-top: 40px;
    padding-top: 20px;
    border-top: 1px solid rgba(212, 175, 55, 0.2);
}}
</style>
</head>
<body>

<!-- MODAL LIGHTBOX INTERATIVO -->
<div id="imageModal" class="modal" onclick="closeModal()">
    <span class="modal-close" onclick="closeModal()">&times;</span>
    <img class="modal-content" id="modalImg" onclick="event.stopPropagation()">
    <div id="modalCaption" class="modal-caption"></div>
</div>

<div class="manual-wrapper">

    <!-- CAPA -->
    <div class="cover-page">
        <div class="cover-badge">SymphonyRecomp • Edição Definitiva (.NET 10)</div>
        
        <div class="cover-title-container">
            <h1>CASTLEVANIA: SOTN</h1>
            <div class="cover-subtitle">Manual do Jogador & Guia Completo</div>
            <p class="cover-tagline">
                A obra-prima de Koji Igarashi recompilada estaticamente para C# nativo em 60+ FPS, Widescreen 16:9, TrueColor 32-bit e a expansão definitiva <strong>SOTN Ultimate</strong> com todos os conteúdos de Sega Saturn e PSP, Multiplayer Co-op & PvP e novo arsenal.
            </p>
        </div>

        <div class="cover-features-grid">
            <div class="cover-feat-box">
                <h4>🎭 6 Personagens & Estilos</h4>
                <p>Alucard, Maria (Saturn), Maria (PSP), Richter clássico, Richter Saturn Coat e Richter Couro Marrom.</p>
            </div>
            <div class="cover-feat-box">
                <h4>🌐 Multiplayer Co-op & PvP</h4>
                <p>Netplay P2P 60Hz em telas desacopladas, Mini-Mapa compartilhado, Buddy Warp e Duelos no Coliseu.</p>
            </div>
            <div class="cover-feat-box">
                <h4>🏰 Conteúdo Saturn & PSP</h4>
                <p>Underground Garden, Cursed Prison, Alucard Spear, Botas Godspeed, músicas exclusivas e arsenal novo.</p>
            </div>
        </div>

        <div class="cover-footer">
            Desenvolvido com paixão pela comunidade BlackLabelHQ & Contribuidores • Edição Atualizada 2026
        </div>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 1: INTRODUÇÃO E ARQUITETURA -->
    <h2 class="section-title">1. Visão Geral & Arquitetura do SymphonyRecomp</h2>

    <div class="card card-purple">
        <div class="card-title">O Que é a Recompilação Estática?</div>
        <p>
            O <strong>SymphonyRecomp</strong> traduziu diretamente todas as ~15.000 rotinas binárias originais em MIPS do PlayStation 1 para C# puro executável sob o runtime moderno do .NET 10. Em vez de emulação tradicional com gargalos de ciclo de instrução, o jogo é compilado nativamente para x64 com acesso direto à GPU (OpenGL 4.5 Core), execução multithread desacoplada e latência zero de entrada.
        </p>
    </div>

    <div class="cover-features-grid" style="margin-bottom: 25px;">
        <div class="card" style="margin-bottom: 0;">
            <div class="card-title">🖥️ Widescreen 16:9 Real</div>
            <p style="font-size: 14px; color: #b8b3c6;">Campo de visão estendido para proporção panorâmica moderna sem esticar nem deformar os sprites clássicos, com renderização contínua de cenários e HUD reposicionado dinamicamente.</p>
        </div>
        <div class="card" style="margin-bottom: 0;">
            <div class="card-title">🎨 TrueColor 32-bit</div>
            <p style="font-size: 14px; color: #b8b3c6;">Eliminação do padrão de dithering quadriculado do PS1. Efeitos de névoa, feixes de luz, chamas e magias possuem agora gradientes suaves e profundidade de cor de 32 bits.</p>
        </div>
        <div class="card" style="margin-bottom: 0;">
            <div class="card-title">⚡ Zero Loading</div>
            <p style="font-size: 14px; color: #b8b3c6;">As portas de transição "CD Loading" e mudanças de área do castelo abrem de forma instantânea sem pausas na taxa de transferência do seu SSD/NVMe.</p>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 2: ATALHOS E MAPEAMENTOS COMPLETOS -->
    <h2 class="section-title">2. Tabela Mestra de Controles & Mapeamento de Teclas</h2>
    <p style="font-size: 16px; margin-bottom: 12px;">
        O SymphonyRecomp aceita simultaneamente comandos via <strong>Teclado</strong> e <strong>Controles USB / Bluetooth</strong> (Xbox One/Series, DualShock 4, DualSense, Nintendo Switch Pro e genéricos DirectInput/XInput):
    </p>

    <table>
        <thead>
            <tr>
                <th style="width: 25%;">Ação no Jogo</th>
                <th style="width: 22%;">Teclado</th>
                <th style="width: 23%;">Controle (Xbox)</th>
                <th style="width: 30%;">Efeito & Detalhes</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Movimentação</strong></td>
                <td><kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> ou <kbd>Setas</kbd></td>
                <td><span class="pad-btn">D-Pad</span> ou <span class="pad-btn">Analógico Esq</span></td>
                <td>Andar, agachar, mirar subweapons e interações.</td>
            </tr>
            <tr>
                <td><strong>Pulo</strong></td>
                <td><kbd>Espaço</kbd> ou <kbd>K</kbd></td>
                <td><span class="pad-btn">A</span> (ou <span class="pad-btn">✕</span> no PS)</td>
                <td>Pulo normal (e duplo/triplo dependendo do personagem).</td>
            </tr>
            <tr>
                <td><strong>Ataque Principal</strong></td>
                <td><kbd>J</kbd></td>
                <td><span class="pad-btn">X</span> (ou <span class="pad-btn">▢</span> no PS)</td>
                <td>Mão Direita (Alucard) / Chicote (Richter) / Pombas (Maria).</td>
            </tr>
            <tr>
                <td><strong>Ataque Secundário / Escudo</strong></td>
                <td><kbd>U</kbd></td>
                <td><span class="pad-btn">B</span> (ou <span class="pad-btn">◯</span> no PS)</td>
                <td>Mão Esquerda (Alucard) / Alternar Subweapon (Maria PSP).</td>
            </tr>
            <tr>
                <td><strong>Arma Secundária (Subweapon)</strong></td>
                <td><kbd>I</kbd> ou <kbd>W</kbd>+<kbd>J</kbd></td>
                <td><span class="pad-btn">Y</span> ou <span class="pad-btn">Cima</span>+<span class="pad-btn">X</span></td>
                <td>Dispara Faca, Machado, Água Benta, Cruz, Bíblia, etc.</td>
            </tr>
            <tr>
                <td><strong>Troca Rápida de Personagem</strong></td>
                <td><kbd>C</kbd></td>
                <td><span class="pad-btn">L3</span> ou <span class="pad-btn">R3</span> (Clique Analógico)</td>
                <td><strong>Hot-Swap em Tempo Real:</strong> Alterne ciclicamente entre os 5 personagens instantaneamente!</td>
            </tr>
            <tr>
                <td><strong>Troca Rápida de Armas</strong></td>
                <td><kbd>Q</kbd></td>
                <td><span class="pad-btn">R3</span> (quando com Alucard)</td>
                <td>Alterne entre 3 conjuntos de armas no inventário sem abrir o menu.</td>
            </tr>
            <tr>
                <td><strong>Corrida / Dash do Alucard</strong></td>
                <td><kbd>→</kbd> <kbd>→</kbd> ou segurar <kbd>Shift</kbd></td>
                <td><kbd>→</kbd> <kbd>→</kbd> ou segurar <span class="pad-btn">L1</span> / <span class="pad-btn">R1</span></td>
                <td>Dispara em alta velocidade pelas salas (Godspeed Boots).</td>
            </tr>
            <tr>
                <td><strong>Magias Rápidas 1 a 6</strong></td>
                <td><kbd>1</kbd> até <kbd>6</kbd></td>
                <td>Menu de Magias (<kbd>F11</kbd>)</td>
                <td>Conjura feitiços e invocações com 1 toque sem motion inputs!</td>
            </tr>
            <tr>
                <td><strong>Barra de Menus ImGui</strong></td>
                <td><kbd>F11</kbd> ou Mouse no Topo</td>
                <td>Menu Superior</td>
                <td>Abre o painel com Fast Travel, Bestiário, Bosses, Mods e Áudio.</td>
            </tr>
            <tr>
                <td><strong>Alternar Tela Cheia</strong></td>
                <td><kbd>Alt</kbd>+<kbd>Enter</kbd> ou <kbd>F</kbd></td>
                <td>Menu Display</td>
                <td>Alterna entre Modo Janela e Tela Cheia sem bordas.</td>
            </tr>
        </tbody>
    </table>

    <div class="page-break"></div>

    <!-- SEÇÃO 3: OS 5 PERSONAGENS JOGÁVEIS -->
    <h2 class="section-title">3. Os 5 Protagonistas Jogáveis (SOTN Ultimate)</h2>
    <p style="font-size: 16px; margin-bottom: 22px;">
        Pressione a tecla <kbd>C</kbd> (ou clique o analógico <span class="pad-btn">L3</span>/<span class="pad-btn">R3</span>) a qualquer instante durante a partida para alternar entre os personagens. <em>(Dica: Clique em qualquer captura para abrir em tela cheia com zoom!)</em>
    </p>

    <!-- 3.1 MARIA SATURN -->
    <div class="char-card no-break">
        <div class="char-header">
            <div class="char-name">3.1 Maria Renard (Sega Saturn Autêntica)</div>
            <div class="char-badge">EXCLUSIVO SEGA SATURN</div>
        </div>
        <p class="char-desc">
            A versão autêntica do Sega Saturn (1998), portada diretamente do binário <code>MARIA.BIN</code>. Maria é acrobática, veloz e conta com ataques de pombas mágicas e a invocação das Quatro Bestas Sagradas orientais.
        </p>

        <ul class="bullet-list">
            <li><span class="highlight">HUD Próprio Autêntico:</span> Exibe a moldura de vinhas verdes com a placa dourada <strong>MARIA</strong> e as barras verticais de HP e MP do Saturn (injetadas dinamicamente de <code>BIN/F_GAME2.BIN</code>).</li>
            <li><span class="highlight">Pulo Triplo Celestial:</span> Pode saltar até três vezes consecutivas no ar, restaurando física aérea.</li>
            <li><span class="highlight">Disparo de Pombas & Ataque de Carga (3 Níveis):</span> Pressione ataque para disparar pombas. Segure para carregar até 3 níveis com brilho correspondente na placa MARIA!</li>
            <li><span class="highlight">As Quatro Bestas Sagradas & Magias Rápidas (<kbd>1</kbd> a <kbd>6</kbd>):</span>
                <br>• <kbd>1</kbd> <strong>Soco Guardião (Guardian Knuckle):</strong> Rajada supersônica de socos devastadores.
                <br>• <kbd>2</kbd> <strong>Suzaku (O Pássaro de Fogo):</strong> Invocação da fênix flamejante que incinera toda a sala.
                <br>• <kbd>3</kbd> <strong>Genbu (A Tartaruga Protetora):</strong> Cura massiva de HP e escudo defensivo divino.
                <br>• <kbd>4</kbd> <strong>Byakko (O Tigre Branco dos Raios):</strong> Relâmpagos sagrados que paralisam o tempo.
                <br>• <kbd>5</kbd> <strong>Seiryuu (O Dragão Azul dos Mares):</strong> Dragão colossal que devora os inimigos na tela.
                <br>• <kbd>6</kbd> <strong>Hino Supremo das Quatro Bestas:</strong> As 4 divindades invocadas juntas!
            </li>
        </ul>

        <h4 style="color: #fce899; margin-top: 15px; font-size: 15px;">Capturas Reais de Gameplay no Castelo (Clique para Ampliar):</h4>
        <div class="gameplay-grid">
            <div class="gameplay-card">
                <img src="{img_ms_idle}" alt="Maria Saturn - HUD Autêntico de Vinhas e Placa MARIA" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">HUD Autêntico do Saturn</span>
                <span class="gameplay-sublabel">Placa dourada MARIA, vinhas e barras verticais de HP e MP</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_ms_atk}" alt="Maria Saturn - Disparo Místico de Pombas" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Disparo Místico de Pombas</span>
                <span class="gameplay-sublabel">Projéteis velozes disparados em arcos horizontais</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_ms_jump}" alt="Maria Saturn - Pulo Triplo Acrobático no Ar" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Pulo Triplo Acrobático</span>
                <span class="gameplay-sublabel">Atinge locais altos e segredos sem necessidade de relíquias</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_ms_charge}" alt="Maria Saturn - Ataque de Carga Nível 3" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Ataque de Carga Nível 3</span>
                <span class="gameplay-sublabel">Letras da placa brilhando com carga máxima acumulada</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_ms_suzaku}" alt="Maria Saturn - Especial Suzaku (Fênix de Fogo)" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Especial: Suzaku (Fênix)</span>
                <span class="gameplay-sublabel">Ave sagrada de chamas cobrindo a tela inteira</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_ms_seiryuu}" alt="Maria Saturn - Especial Seiryuu (Dragão Azul)" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Especial: Seiryuu (Dragão Azul)</span>
                <span class="gameplay-sublabel">O dragão divino dos mares varrendo todos os monstros</span>
            </div>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- 3.2 MARIA PSP -->
    <div class="char-card no-break">
        <div class="char-header">
            <div class="char-name">3.2 Maria Renard (PSP - The Dracula X Chronicles)</div>
            <div class="char-badge badge-psp">DRACULA X CHRONICLES</div>
        </div>
        <p class="char-desc">
            A versão refinada e tática do remake de PSP (2007). Combate com chutes ágeis, projéteis velozes e o sistema exclusivo de <strong>Duplo Arsenal de Subweapons</strong> com alternância instantânea.
        </p>

        <ul class="bullet-list">
            <li><span class="highlight">Sistema de Duplo Subweapon:</span> Maria PSP carrega duas armas secundárias ativas simultaneamente! Pressione <kbd>U</kbd> ou <span class="pad-btn">◯</span> / <span class="pad-btn">B</span> para alternar instantaneamente entre elas.</li>
            <li><span class="highlight">Invocação de Familiar com Triângulo (<kbd>I</kbd> / <span class="pad-btn">△</span> / <span class="pad-btn">Y</span>):</span> Dispara o Cardeal, Gato Byakko, Tartaruga ou Dragão dependendo do subweapon equipado.</li>
            <li><span class="highlight">HUD Dual-Icon Exclusivo:</span> No canto superior esquerdo são exibidos os dois slots de subweapons ativos e a reserva de corações.</li>
        </ul>

        <h4 style="color: #fce899; margin-top: 15px; font-size: 15px;">Capturas Reais de Gameplay no Castelo (Clique para Ampliar):</h4>
        <div class="gameplay-grid">
            <div class="gameplay-card">
                <img src="{img_mp_idle}" alt="Maria PSP - Postura de Combate no Castelo" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Postura Ágil de PSP</span>
                <span class="gameplay-sublabel">Sprite refinado de Dracula X Chronicles</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_mp_hud}" alt="Maria PSP - HUD Dual-Icon com 2 Subweapons" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">HUD Dual-Icon</span>
                <span class="gameplay-sublabel">Mostra as duas armas secundárias prontas para troca imediata</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_mp_atk}" alt="Maria PSP - Chutes Acrobáticos e Projéteis" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Chutes Acrobáticos</span>
                <span class="gameplay-sublabel">Combate corporal ágil com disparo de projéteis</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_mp_spec}" alt="Maria PSP - Invocação Espiritual com Botão Triângulo" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Invocação com Triângulo</span>
                <span class="gameplay-sublabel">Libera o familiar sagrado correspondente ao subweapon</span>
            </div>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- 3.3 RICHTER CLÁSSICO, SATURN COAT E COURO MARROM -->
    <div class="char-card no-break">
        <div class="char-header">
            <div class="char-name">3.3 Richter Belmont (Clássico, Saturn Coat & Sobretudo Couro Marrom)</div>
            <div class="char-badge badge-coat">3 FIGURINOS SELECIONÁVEIS</div>
        </div>
        <p class="char-desc">
            O lendário caçador de vampiros com seu chicote sagrado Vampire Killer. O SymphonyRecomp oferece 3 figurinos distintos: o visual clássico de colete azul do PS1, o sobretudo azul com detalhes dourados do Sega Saturn (<em>RIC2</em>) e o novo sobretudo rústico de couro marrom inspirado no visual clássico de Simon e Trevor Belmont!
        </p>

        <ul class="bullet-list">
            <li><span class="highlight">Chicote Vampire Killer:</span> Ataque chicoteando à frente ou segure o botão de ataque e gire o direcional para girar o chicote livremente, criando um escudo refletor de projéteis!</li>
            <li><span class="highlight">Acrobacias & Artes Marciais:</span> Pulo duplo com mortal para trás, Carrinho (<kbd>↓</kbd> + <kbd>Pulo</kbd>), Super Pulo (<kbd>↓</kbd> <kbd>↑</kbd> + <kbd>Pulo</kbd>) e a investida supersônica <em>Blade Dash</em> (<kbd>↓</kbd> <kbd>↘</kbd> <kbd>→</kbd> + <kbd>Ataque</kbd>).</li>
            <li><span class="highlight">Os 5 Grandes Item Crashes:</span>
                <br>• <strong>Hydro Storm (Água Benta - 15 Corações):</strong> A chuva sagrada torrencial que purifica qualquer sala.
                <br>• <strong>Grand Cross (Cruz Sagrada - 20 Corações):</strong> Pilar maciço de crucifixos radiantes com invulnerabilidade total.
                <br>• <strong>Thousand Blades (Faca - 10 Corações):</strong> Saraivada impiedosa de punhais de prata.
                <br>• <strong>Bíblia Sagrada & Agunea (10-15 Corações):</strong> Escrituras sagradas orbitantes e relâmpagos dirigidos.
            </li>
        </ul>

        <h4 style="color: #fce899; margin-top: 15px; font-size: 15px;">Capturas Reais de Gameplay no Castelo (Clique para Ampliar):</h4>
        <div class="gameplay-grid">
            <div class="gameplay-card">
                <img src="{img_rc_idle}" alt="Richter Belmont - Visual Clássico de Colete" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Richter Clássico (Colete)</span>
                <span class="gameplay-sublabel">O caçador tradicional do prólogo de Symphony of the Night</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_rcoat_idle}" alt="Richter Belmont - Sobretudo Saturn Coat com Bordados Dourados" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Richter Saturn Coat</span>
                <span class="gameplay-sublabel">Sobretudo longo azul com detalhes dourados do Sega Saturn</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_rbrown_gameplay}" alt="Richter Belmont - Sobretudo Couro Marrom Estilo Simon Belmont" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Richter Couro Marrom</span>
                <span class="gameplay-sublabel">Sobretudo de couro rústico estilo Simon/Trevor Belmont</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_rbrown_idle}" alt="Comparativo dos 3 Trajes de Richter Belmont" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Os 3 Trajes de Richter</span>
                <span class="gameplay-sublabel">Colete clássico, Sobretudo Saturn e Couro Simon</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_rc_whip}" alt="Richter Belmont - Chicotada Vampire Killer Estendida" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Chicote Vampire Killer</span>
                <span class="gameplay-sublabel">Alcance devastador e rotação contínua multidirecional</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_rc_hydro}" alt="Richter Belmont - Item Crash Hydro Storm" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Crash: Hydro Storm</span>
                <span class="gameplay-sublabel">Chuva torrencial de água benta eliminando tudo na tela</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_rc_grand}" alt="Richter Belmont - Item Crash Grand Cross" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Crash: Grand Cross</span>
                <span class="gameplay-sublabel">Coluna monumental de luz sagrada com invencibilidade</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_rc_dash}" alt="Richter Belmont - Arte Marcial Blade Dash" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Arte Marcial: Blade Dash</span>
                <span class="gameplay-sublabel">Investida horizontal supersônica cortando as defesas inimigas</span>
            </div>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- 3.4 ALUCARD -->
    <div class="char-card no-break">
        <div class="char-header">
            <div class="char-name">3.4 Alucard (O Herdeiro Imortal de Drácula)</div>
            <div class="char-badge badge-alu">PROTAGONISTA</div>
        </div>
        <p class="char-desc">
            O protagonista dhampir imortal com todo o sistema de RPG, equipamentos, relíquias, armas clássicas (Crissaegrim, Shield Rod, Alucard Sword) e as lendárias magias de sangue conjuráveis instantaneamente.
        </p>

        <ul class="bullet-list">
            <li><span class="highlight">Widescreen 16:9 Nativo:</span> O castelo se expande em visão panorâmica perfeita sem deformar a silhueta ou as proporções de Alucard.</li>
            <li><span class="highlight">Troca Rápida de Armas (<kbd>Q</kbd> / <span class="pad-btn">R3</span>):</span> Alterne entre até 3 conjuntos de armas e escudos sem interromper a exploração abrindo menus.</li>
            <li><span class="highlight">As 5 Magias de Sangue (Atalhos <kbd>1</kbd> a <kbd>5</kbd>):</span>
                <br>• <kbd>1</kbd> <strong>Summon Spirit (Espírito Perseguidor - 5 MP):</strong> Invoca um espírito que busca e ataca o inimigo mais próximo.
                <br>• <kbd>2</kbd> <strong>Tetra Spirit (Quatro Espíritos - 20 MP):</strong> Quatro espíritos vorazes disparados em leque.
                <br>• <kbd>3</kbd> <strong>Dark Metamorphosis (Metamorfose Sombria - 10 MP):</strong> Absorve o sangue dos inimigos feridos para curar o HP de Alucard.
                <br>• <kbd>4</kbd> <strong>Hellfire (Teleporte com Bolas de Fogo - 15 MP):</strong> Alucard se envolve na capa, teleporta e dispara projéteis infernais.
                <br>• <kbd>5</kbd> <strong>Soul Steal (Roubo de Alma - 50 MP):</strong> A magia suprema que drena a energia vital de todos os inimigos da sala!
            </li>
        </ul>

        <h4 style="color: #fce899; margin-top: 15px; font-size: 15px;">Capturas Reais de Gameplay no Castelo (Clique para Ampliar):</h4>
        <div class="gameplay-grid">
            <div class="gameplay-card">
                <img src="{img_al_idle}" alt="Alucard - Widescreen 16:9 no Castelo" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Widescreen 16:9 Nativo</span>
                <span class="gameplay-sublabel">Visão panorâmica sem distorção com HUD adaptativo</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_al_atk}" alt="Alucard - Golpe de Espada e Dinâmica de Capa" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Golpe de Espada & Capa</span>
                <span class="gameplay-sublabel">Animação suave de capa com paletas originais completas</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_al_summon}" alt="Alucard - Magia Summon Spirit" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Magia: Summon Spirit</span>
                <span class="gameplay-sublabel">Espírito espectral teleguiado de baixo consumo de MP</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_al_tetra}" alt="Alucard - Magia Tetra Spirit" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Magia: Tetra Spirit</span>
                <span class="gameplay-sublabel">Disparo simultâneo de 4 espíritos teleguiados</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_al_dark}" alt="Alucard - Magia Dark Metamorphosis" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Magia: Dark Metamorphosis</span>
                <span class="gameplay-sublabel">Aura carmesim ativada para cura instantânea ao contato com sangue</span>
            </div>
            <div class="gameplay-card">
                <img src="{img_al_soul}" alt="Alucard - Magia Suprema Soul Steal" onclick="openModal(this.src, this.alt)">
                <span class="gameplay-label">Magia Suprema: Soul Steal</span>
                <span class="gameplay-sublabel">Ondas de almas absorvidas de toda a sala com recuperação massiva de vida</span>
            </div>
        </div>
    </div>

    <!-- 3.5 SOTN ULTIMATE & QOL MASTER -->
    <div class="char-card no-break" style="margin-top: 25px;">
        <div class="char-header">
            <div class="char-name">3.5 Recursos Mestres do SOTN Ultimate & QoL</div>
            <div class="char-badge badge-coat">EXCLUSIVO PC</div>
        </div>
        <p class="char-desc">
            O SymphonyRecomp integra o conjunto definitivo de expansões e correções do ecossistema clássico:
        </p>
        <ul class="bullet-list">
            <li><span class="highlight">Bypass da Morte (Death Skip Oficial):</span> Pule a cutscene da Morte na Entrada do Castelo pelo menu <span class="pad-btn">F11</span>. O Alucard mantém intacto todo o seu arsenal supremo inicial (Alucard Sword, Shield, Dragon Helm, Alucard Mail, Twilight Cloak e Anel de Ares) e o corredor fica desobstruído!</li>
            <li><span class="highlight">Roda de Habilidades Adaptativa:</span> Adaptação dinâmica para qualquer personagem com atalhos numéricos <kbd>1</kbd> a <kbd>6</kbd> (Magias de Sangue para Alucard, Item Crashes e Artes Marciais para Richter, Invocações Sagradas para Maria).</li>
            <li><span class="highlight">Arsenal do Sega Saturn:</span> Botas da Velocidade (God Speed Shoes), Terceira Mão / Holder (L2) e Efeito Dinâmico de Cor no Manto do Arco-Íris.</li>
            <li><span class="highlight">Tríplice Seleção de Dublagem:</span> Alterne em tempo real entre a dublagem da PSP (Dracula X Chronicles), a dublagem clássica de PS1 ("What is a man?") e o áudio japonês original com legendas em português.</li>
        </ul>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 4: MULTIPLAYER CO-OP & PVP -->
    <h2 class="section-title">4. Multiplayer Cooperativo & Duelos PvP (Netplay P2P)</h2>

    <div class="card card-emerald">
        <div class="card-title">Arquitetura de Telas Desacopladas & Sincronização UDP 60Hz</div>
        <p>
            O SymphonyRecomp introduz um modo multiplayer verdadeiramente revolucionário para Castlevania: SOTN. Ao contrário de ports antigos com tela dividida ou travamento de câmera na mesma sala, o motor opera com <strong>câmeras completamente desacopladas</strong> via conexão P2P UDP de baixa latência a 60Hz. Cada jogador explora o castelo em sua própria tela e taxa de quadros independente!
        </p>
    </div>

    <div class="cover-features-grid" style="grid-template-columns: 1fr 1fr; margin-bottom: 25px;">
        <div class="card">
            <div class="card-title">🤝 Co-op: Exploração & Buddy Warp</div>
            <ul class="bullet-list" style="margin-bottom: 0;">
                <li><strong>Marionete Remota (Puppet System):</strong> O jogador parceiro é renderizado no seu jogo com sprites, paletas, animações de subweapons e sons exatos.</li>
                <li><strong>Mini-Mapa & Radar Compartilhado:</strong> Veja em tempo real em qual sala do castelo seu aliado está explorando através do ícone no radar.</li>
                <li><strong>Buddy Warp:</strong> Pressione o atalho de teleporte para viajar instantaneamente para a sala exata onde seu amigo se encontra!</li>
            </ul>
        </div>
        <div class="card card-red">
            <div class="card-title">⚔️ PvP: Duelos de Caçadores no Coliseu</div>
            <ul class="bullet-list" style="margin-bottom: 0;">
                <li><strong>Colosseum Battle Arena:</strong> A arena do Coliseu foi convertida em um palco épico de duelos diretos entre os 5 personagens jogáveis.</li>
                <li><strong>Cálculo Real de Dano:</strong> Espadas, chicotadas, projéteis, magias e invocações causam dano real com caixas de colisão sincronizadas.</li>
                <li><strong>Placar de Vitórias:</strong> Registro de rounds com contagem de KOs e reinício rápido de batalha sem telas de game over.</li>
            </ul>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 5: CONTEÚDOS EXCLUSIVOS SATURN & PSP -->
    <h2 class="section-title">5. Conteúdos Exclusivos dos Dois Ports (Saturn & PSP)</h2>

    <div class="card card-gold">
        <div class="card-title">O Melhor de Todas as Edições em um Único Jogo</div>
        <p>
            O mod SOTN Ultimate unifica o legado do Sega Saturn japonês de 1998 e do PSP de 2007 dentro da base sólida de PS1 do SymphonyRecomp. Abaixo estão as áreas, itens e áudios exclusivos incorporados:
        </p>
    </div>

    <table>
        <thead>
            <tr>
                <th style="width: 28%;">Recurso Exclusivo</th>
                <th style="width: 22%;">Origem</th>
                <th style="width: 50%;">Descrição & Localização no Castelo</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Underground Garden</strong></td>
                <td>Sega Saturn</td>
                <td>Nova área acessível pela entrada do castelo. Florestas subterrâneas repletas de vegetações venenosas, novas armadilhas e inimigos exclusivos como <em>Gardener</em> e <em>Skeleton Tree</em>.</td>
            </tr>
            <tr>
                <td><strong>Cursed Prison</strong></td>
                <td>Sega Saturn</td>
                <td>Masmorras sombrias localizadas abaixo das catacumbas com atmosfera claustrofóbica, novos chefes e quebra-cabeças de alavancas.</td>
            </tr>
            <tr>
                <td><strong>Alucard Spear</strong></td>
                <td>Saturn & Bloodlines</td>
                <td>A famosa lança de Eric Lecarde empunhável por Alucard com alcance longo, dano de perfuração e golpe especial giratório.</td>
            </tr>
            <tr>
                <td><strong>Botas Godspeed (Dash)</strong></td>
                <td>Sega Saturn</td>
                <td>Permitem que Alucard corra continuamente pelas salas sem precisar de comandos complexos de dash repetitivos.</td>
            </tr>
            <tr>
                <td><strong>Rainbow Cloak</strong></td>
                <td>Sega Saturn</td>
                <td>Capa mágica que altera ciclicamente as cores do forro e da silhueta de Alucard em espectro cromático suave.</td>
            </tr>
            <tr>
                <td><strong>Trilha Sonora Inédita</strong></td>
                <td>Saturn & PSP</td>
                <td>Faixas originais como <em>Chaconne in C Minor</em>, <em>Vampire Killer (Remix)</em>, <em>Bloody Tears</em> e a cantiga secreta <em>Nocturne</em> em japonês com a fada familiar.</td>
            </tr>
        </tbody>
    </table>

    <div class="page-break"></div>

    <!-- SEÇÃO 6: VIAGEM RÁPIDA E MAPAS -->
    <h2 class="section-title">6. Viagem Rápida Global & Os 28 Destinos Homologados</h2>

    <div class="card">
        <div class="card-title">🗺️ Teleporte Preciso sem Risco de Queda de Mapa</div>
        <p>
            O painel de <strong>Viagem Rápida (Fast Travel)</strong> do menu de mods (<kbd>F11</kbd>) permite viajar instantaneamente entre <strong>28 destinos estratégicos</strong> em ambos os castelos (Castelo Normal e Castelo Invertido). Cada coordenada foi rigorosamente aferida com o jogador em pé no piso da sala (<code>vy = 0</code>), garantindo transição suave e física perfeita com qualquer um dos 5 personagens.
        </p>
    </div>

    <div class="char-card no-break">
        <h4 style="color: #fce899; margin-bottom: 12px; font-size: 16px;">Mapa Oficial dos 28 Destinos Homologados (Clique para Ampliar):</h4>
        <div style="background: #000; border: 2px solid #d4af37; border-radius: 8px; padding: 10px; text-align: center;">
            <img src="{img_teleport_destinos}" alt="Mapa dos 28 Destinos do Fast Travel" class="clickable-thumb" style="width: 100%; max-height: 480px; object-fit: contain; cursor: zoom-in;" onclick="openModal(this.src, this.alt)">
            <span style="display: block; font-size: 13px; color: #fce899; margin-top: 8px; font-family: 'Cinzel', serif;">
                🔍 Clique na imagem acima para inspecionar os 28 pontos em alta resolução
            </span>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 7: COMPROVAÇÃO MULTIPLATAFORMA -->
    <h2 class="section-title">7. Validação Multiplataforma: Windows 11 vs Linux (WSL)</h2>

    <div class="card card-emerald">
        <div class="card-title">Paridade Gráfica Absoluta em Ambas as Plataformas</div>
        <p>
            O SymphonyRecomp foi testado e validado em ambos os ambientes, demonstrando paridade gráfica 100% idêntica na renderização dos overlays, paletas de cores, HUDs customizados e efeitos de magias.
        </p>
    </div>

    <table>
        <thead>
            <tr>
                <th style="width: 25%;">Parâmetro</th>
                <th style="width: 38%;">Windows 11 Nativo</th>
                <th style="width: 37%;">Linux (WSL Ubuntu 24.04)</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Status de Execução</strong></td>
                <td><strong style="color: #4ade80;">✅ 100% Funcional (103/103 Ações)</strong></td>
                <td><strong style="color: #4ade80;">✅ 100% Funcional (Sequência Completa)</strong></td>
            </tr>
            <tr>
                <td><strong>Pipeline Gráfico</strong></td>
                <td>NVIDIA GeForce RTX 3060 (OpenGL 4.5 Core nativo)</td>
                <td>Mesa llvmpipe (OpenGL 4.5 Core rasterizado via CPU)</td>
            </tr>
            <tr>
                <td><strong>Desempenho / Taxa</strong></td>
                <td>Instantâneo (144 Hz / 60 FPS com aceleração de hardware)</td>
                <td>Suave (60 Hz com rasterização por software no WSL)</td>
            </tr>
            <tr>
                <td><strong>Resolução de Render</strong></td>
                <td>1280x720 Widescreen 16:9 Nativo</td>
                <td>1280x720 Widescreen 16:9 Nativo</td>
            </tr>
            <tr>
                <td><strong>Detecção do Ultimate</strong></td>
                <td>Injeção automática do HUD da Maria de <code>disc/</code></td>
                <td>Injeção automática do HUD da Maria de <code>disc/</code></td>
            </tr>
        </tbody>
    </table>

    <h4 style="color: #fce899; margin-top: 20px; font-size: 15px;">Capturas Comparativas em Tempo Real no Linux WSL (Clique para Ampliar):</h4>
    <div class="gameplay-grid">
        <div class="gameplay-card">
            <img src="{img_lx_maria}" alt="Maria Saturn no Linux WSL com HUD Autêntico" onclick="openModal(this.src, this.alt)">
            <span class="gameplay-label">Maria Saturn (Linux WSL)</span>
            <span class="gameplay-sublabel">Mesa OpenGL 4.5 Core com HUD de vinhas autêntico</span>
        </div>
        <div class="gameplay-card">
            <img src="{img_lx_psp}" alt="Maria PSP no Linux WSL com HUD Dual-Icon" onclick="openModal(this.src, this.alt)">
            <span class="gameplay-label">Maria PSP (Linux WSL)</span>
            <span class="gameplay-sublabel">Dual subweapon renderizado com fidelidade perfeita</span>
        </div>
        <div class="gameplay-card">
            <img src="{img_lx_richter}" alt="Richter Clássico no Linux WSL" onclick="openModal(this.src, this.alt)">
            <span class="gameplay-label">Richter Clássico (Linux WSL)</span>
            <span class="gameplay-sublabel">Caçador clássico pronto para combate no castelo</span>
        </div>
        <div class="gameplay-card">
            <img src="{img_lx_coat}" alt="Richter Saturn Coat no Linux WSL" onclick="openModal(this.src, this.alt)">
            <span class="gameplay-label">Richter Saturn Coat (Linux WSL)</span>
            <span class="gameplay-sublabel">Sobretudo longo azul com ornamentos dourados</span>
        </div>
        <div class="gameplay-card">
            <img src="{img_lx_alucard}" alt="Alucard no Linux WSL em Widescreen" onclick="openModal(this.src, this.alt)">
            <span class="gameplay-label">Alucard (Linux WSL)</span>
            <span class="gameplay-sublabel">Alucard em proporção 16:9 panorâmica pura</span>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 8: QUALIDADE DE VIDA E DIFICULDADE HARDCORE -->
    <h2 class="section-title">8. Opções de Qualidade de Vida (QoL) & Dificuldade</h2>

    <div class="card">
        <div class="card-title">⚙️ Melhorias de Qualidade de Vida (QoL Settings)</div>
        <ul class="bullet-list">
            <li><strong>Pular Salas de CD:</strong> Atravessa portas de transição sem qualquer pausa de carregamento.</li>
            <li><strong>Música Secreta da Fada em Japonês:</strong> Habilita a canção <em>Nocturne</em> que a fada familiar cantava exclusivamente na versão japonesa.</li>
            <li><strong>Controles Aprimorados de Richter:</strong> Possibilidade de disparar Item Crash diretamente pelo botão <span class="pad-btn">△</span> ou <span class="pad-btn">Y</span> sem motion inputs.</li>
            <li><strong>Filtros de Cor e Alto Contraste:</strong> Ajustes para melhor visibilidade de projéteis e status de veneno/maldição.</li>
        </ul>
    </div>

    <div class="card card-red">
        <div class="card-title">☠️ Dificuldade Customizável & Modo HARDCORE</div>
        <ul class="bullet-list">
            <li><strong>Multiplicador de Dano (1.5x a 5.0x):</strong> Dobre ou quintuplique o dano recebido para uma experiência estilo Dark Souls.</li>
            <li><strong>Anti-Spam de Poções:</strong> Impede o uso repetitivo de itens de cura durante lutas de chefes para exigir maestria no combate.</li>
            <li><strong>Modo HARDCORE (Permadeath):</strong> Se a sua vida chegar a zero, o savegame é permanentemente deletado!</li>
        </ul>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 9: CRÉDITOS E RECONHECIMENTO DOS PROJETOS ORIGINAIS -->
    <h2 class="section-title">9. Créditos & Reconhecimento dos Projetos Originais</h2>

    <div class="card card-gold">
        <div class="card-title">📜 Uma Obra Construída Sobre Gigantes</div>
        <p>
            O <strong>SymphonyRecomp</strong> é o fruto da união e dedicação de décadas de pesquisadores, engenheiros reversos, fãs apaixonados e modders da comunidade mundial de Castlevania. Nossos mais profundos agradecimentos a todos os criadores e projetos originais:
        </p>
    </div>

    <div class="credits-grid">
        <div class="credits-block">
            <h4>🏰 Konami & Criadores Originais</h4>
            <ul>
                <li><strong>Koji Igarashi (IGA):</strong> Diretor e produtor visionário que redefiniu o gênero com Castlevania: Symphony of the Night.</li>
                <li><strong>Ayami Kojima:</strong> A lendária artista e ilustradora responsável pelo design barroco inesquecível de Alucard e dos personagens.</li>
                <li><strong>Michiru Yamane:</strong> Compositora da magistral trilha sonora gótica que ecoa pelas salas do castelo.</li>
                <li><strong>Konami Tokyo & Kobe:</strong> Pelos lançamentos originais de <em>Symphony of the Night (PlayStation 1997)</em>, <em>Akumajou Dracula X: Gekka no Yasoukyoku (Sega Saturn 1998)</em> e <em>The Dracula X Chronicles (PSP 2007)</em>.</li>
            </ul>
        </div>

        <div class="credits-block">
            <h4>⚡ BlackLabelHQ & Motor RecompOne</h4>
            <ul>
                <li><strong>flaffymg:</strong> Criador genial do motor de recompilação estática <em>RecompOne</em>, arquiteto-chefe do SymphonyRecomp, sistema de patching dinâmico e compilação de binários.</li>
                <li><strong>Derp Princess:</strong> Engenheira fundamental que mapeou e corrigiu centenas de crashes, funções não mapeadas e comportamento de CPU/GPU, além de idealizadora do Blackout Mod.</li>
                <li><strong>wowjinxy:</strong> Desenvolvedor da versão inicial do SymphonyRecomp, modularização de configuração e infraestrutura do projeto.</li>
            </ul>
        </div>
    </div>

    <div class="credits-grid">
        <div class="credits-block">
            <h4>🔬 SOTN Decomp Project & SOTN Ultimate</h4>
            <ul>
                <li><strong>Xeeynamo & Contribuidores do sotn-decomp:</strong> Pelo hercúleo projeto de descompilação C de SOTN, base absoluta de conhecimento que possibilitou a compreensão do código e o mod <code>mariapsp-forpsx</code>.</li>
                <li><strong>sozud:</strong> Autor do <code>tools/maria-sfx</code> e engenharia reversa do banco de áudio SPU/VAB da Maria de PSP.</li>
                <li><strong>Equipe do SOTN Ultimate Mod (v1.4):</strong> Por fundir e compilar os binários de Sega Saturn e PSP (<code>MARIA.BIN</code>, <code>MARIA2.BIN</code>, <code>RIC2.BIN</code>) com layouts estendidos.</li>
                <li><strong>Knight0fDragon:</strong> Autor da tradução em inglês de Saturn (v1.021) e das descrições do bestiário do Saturn.</li>
            </ul>
        </div>

        <div class="credits-block">
            <h4>🎲 Modding, QoL & Comunidade</h4>
            <ul>
                <li><strong>Mottzilla0:</strong> Pioneiro dos scripts de Randomizer de SOTN, overlays de mapa e arquitetura de salas secretas.</li>
                <li><strong>eldri7ch:</strong> Desenvolvedor do Randomizer integrado e dos aclamados Quality of Life (QoL) Mods.</li>
                <li><strong>Gustavo Clemente (GuhClemente):</strong> Porte de 5 personagens com Hot-Swap em tempo real, HUD autêntico de Saturn para Maria, Fast Travel preciso nos 28 destinos, suíte automatizada de testes e gerador do manual.</li>
                <li><strong>Dr4gonBlitz, JupiterClimb & Testers:</strong> Pela validação em stream e busca incansável por bugs.</li>
            </ul>
        </div>
    </div>

    <div class="page-break"></div>

    <!-- SEÇÃO 10: GUIA RÁPIDO DE SOLUÇÃO DE PROBLEMAS -->
    <h2 class="section-title">10. Guia Rápido de Solução de Problemas</h2>

    <table>
        <thead>
            <tr>
                <th style="width: 28%;">Problema</th>
                <th style="width: 32%;">Causa</th>
                <th style="width: 40%;">Solução</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Tela preta no início</strong></td>
                <td>O <code>settings.json</code> estava apontando diretamente para o disco modificado do Ultimate como CD principal.</td>
                <td>O SymphonyRecomp roda sobre a ISO americana retail (<code>Castlevania - Symphony of the Night (USA).cue</code>) e busca o Ultimate em <code>disc/</code> para injetar os assets. O caminho já foi devidamente corrigido!</td>
            </tr>
            <tr>
                <td><strong>Janela não abre ou sumiu</strong></td>
                <td>Antivírus (Norton, Defender) bloqueando o executável recém-compilado.</td>
                <td>Adicione a pasta <code>C:\\github\\SymphonyRecomp</code> às exclusões do antivírus e dê dois cliques em <kbd>windows_run_no_build.bat</kbd>.</td>
            </tr>
            <tr>
                <td><strong>Como abrir o jogo rápido</strong></td>
                <td>Deseja jogar direto sem aguardar build.</td>
                <td>Execute <kbd>windows_run_no_build.bat</kbd>. O jogo inicia em menos de 1 segundo!</td>
            </tr>
            <tr>
                <td><strong>Como regerar este manual</strong></td>
                <td>Adicionou novas capturas, golpes ou modificou textos.</td>
                <td>Rode no terminal: <code>python generate_manual.py</code>! O PDF e o HTML são regerados em segundos.</td>
            </tr>
        </tbody>
    </table>

    <div class="card card-purple" style="margin-top: 30px;">
        <div class="card-title">🦇 Boa Jornada pelo Castelo!</div>
        <p>
            O SymphonyRecomp representa a celebração definitiva de um dos maiores clássicos da história dos videogames. Esperamos que este manual sirva como seu companheiro definitivo pelas torres e catacumbas do castelo!
        </p>
    </div>

    <div class="chapter-footer">
        SymphonyRecomp Manual Oficial do Jogador • Gerado automaticamente com Chromium Engine
    </div>

</div>

<!-- SCRIPT JS DO LIGHTBOX INTERATIVO -->
<script>
function openModal(src, caption) {{
    var modal = document.getElementById('imageModal');
    var modalImg = document.getElementById('modalImg');
    var captionText = document.getElementById('modalCaption');
    modal.classList.add('active');
    modalImg.src = src;
    captionText.innerHTML = caption || '';
    document.body.style.overflow = 'hidden';
}}

function closeModal() {{
    var modal = document.getElementById('imageModal');
    modal.classList.remove('active');
    document.body.style.overflow = 'auto';
}}

document.addEventListener('keydown', function(e) {{
    if (e.key === 'Escape') {{
        closeModal();
    }}
}});
</script>

</body>
</html>"""
    return html

def main():
    print("==================================================")
    print("🏰 SymphonyRecomp - Gerador de Manual Definitivo")
    print("==================================================")

    print("1. Montando HTML completo com capturas nativas em 1280x720...")
    html_content = build_html()

    print(f"2. Salvando arquivo HTML em: {OUTPUT_HTML}")
    with open(OUTPUT_HTML, "w", encoding="utf-8") as f:
        f.write(html_content)

    print("3. Procurando navegador Chromium (Edge ou Chrome)...")
    browser = find_chromium_browser()
    if not browser:
        print("❌ ERRO: Nenhum navegador Chromium (Edge ou Chrome) foi encontrado no sistema!")
        sys.exit(1)

    print(f"   Navegador encontrado: {browser}")

    print("4. Renderizando PDF via Chromium Headless...")
    cmd = [
        browser,
        "--headless",
        "--disable-gpu",
        "--run-all-compositor-stages-before-draw",
        "--no-pdf-header-footer",
        f"--print-to-pdf={OUTPUT_PDF}",
        str(OUTPUT_HTML.resolve())
    ]

    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0 and not OUTPUT_PDF.exists():
        print(f"❌ Falha ao gerar PDF: {res.stderr}")
        sys.exit(1)

    if OUTPUT_PDF.exists():
        size_kb = OUTPUT_PDF.stat().st_size / 1024
        print(f"✅ SUCESSO! PDF gerado em: {OUTPUT_PDF} ({size_kb:.1f} KB)")
        try:
            import shutil
            shutil.copy2(OUTPUT_PDF, DOCS_PDF)
            print(f"✅ Cópia espelho salva em: {DOCS_PDF}")
        except Exception:
            pass
    else:
        print("❌ Arquivo PDF não foi gerado.")
        sys.exit(1)

    print("==================================================")
    print("✨ Processo concluído com êxito!")

if __name__ == "__main__":
    main()
