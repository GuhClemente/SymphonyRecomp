// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/sound.c (at file scope, in place)
// Saturn port: the Saturn-only music
// (mods/sotn-ultimate/tools/gen_saturn_music.py) has config ids past the retail
// table, whose size can't change (g_SfxScripts right after it sits at a fixed
// address)
#define MOD_XA_MUSIC_TABLE
#include "../../../../src/dra/mod_xa_music.h"
// PSP dub toggle: the retail table keeps the PS1 voices; the PSP English
// recordings are extra streams (mods/sotn-ultimate/tools/repack_xa.py --dub
// writes mod_psp_xa.h: a bit per voice id from MOD_PSP_XA_FIRST that has a PSP
// version, and per set bit, in order, the stream's sector - MOD_PSP_XA_BASE
// (bits 0-14), its length unk228 (15-24) and file (25-30); channel =
// sector % 16, volume / mode as the PS1 config). While MOD_PSP_DUB_ON(), a
// voice id with a PSP version plays it. The JP dub (MOD_JP_DUB_ON) the same
// way from g_ModDub.jp (SEL's copy of src/st/sel/mod_jp_xa.h), with 11 bits
// of length (its lines run to 25.3 s) and 6 of file (26-31), and a version
// of the PSP dub's new line MOD_VO_DID_YOU_NOT; ids without one play the
// PSP / PS1 line as before.
#include "../../../../src/dra/mod_psp_xa.h"
// the place of voice id bit k among the set bits, -1 when clear
static s32 ModXaIndex(const u8* bits, s32 k) {
    s32 n;

    if (k < 0 || k > MOD_PSP_XA_LAST - MOD_PSP_XA_FIRST ||
        !(bits[k >> 3] & (1 << (k & 7)))) {
        return -1;
    }
    n = 0;
    while (--k >= 0) {
        if (bits[k >> 3] & (1 << (k & 7))) {
            n++;
        }
    }
    return n;
}
static const XaMusicConfig* ModXaConfig(s32 i) {
    const XaMusicConfig* c;
    XaMusicConfig* r;
    s32 k, n, len;
    u32 e;

    c = i >= MOD_XA_RETAIL_CONFIGS
            ? &g_ModXaMusicConfigs[i - MOD_XA_RETAIL_CONFIGS]
            : &g_XaMusicConfigs[i];
#ifdef MU_MOD_NOCTURNE_EN
    // the fairy's song with the PSP dub: the PSP's English recording
    // (mods/sotn-ultimate/tools/gen_saturn_music.py ONESHOTS); the JP dub keeps
    // the PS1's. PSP dub = both dub bits clear (MOD_PSP_DUB_ON() &&
    // !MOD_JP_DUB_ON(), in one test: DRA has no room for the two)
    if (i == MU_NOCTURNE - 0x300 &&
        !(MOD_STATUS_FLAGS & (MOD_FLAG_PS1_DUB | MOD_FLAG_JP_DUB))) {
        return &g_ModXaMusicConfigs[MU_MOD_NOCTURNE_EN - 0x300 -
                                    MOD_XA_RETAIL_CONFIGS];
    }
#endif
    k = i - (MOD_PSP_XA_FIRST - 0x300);
    if (MOD_JP_DUB_ON() && g_ModDub.jp.magic == MOD_JP_MAGIC) {
        e = 0;
#ifdef MOD_VO_DID_YOU_NOT
        if (i == MOD_VO_DID_YOU_NOT - 0x300) {
            e = g_ModDub.jp.didYouNot;
        }
#endif
        n = ModXaIndex(g_ModDub.jp.xaBits, k);
        if (n >= 0) {
            e = g_ModDub.jp.xa[n];
        }
        if (e) {
            k = g_ModDub.jp.xaBase;
            len = 11;
            goto build;
        }
    }
    if (i >= MOD_XA_RETAIL_CONFIGS || !MOD_PSP_DUB_ON()) {
        return c;
    }
    n = ModXaIndex(g_ModPspXaBits, k);
    if (n < 0) {
        return c;
    }
    e = g_ModPspXa[n];
    k = MOD_PSP_XA_BASE;
    len = 10;
build:
    r = (XaMusicConfig*)g_ModDub.cfg;
    *r = *c;
    k += e & 0x7FFF;
    r->cd_addr = k & ~15;
    r->filter_channel_id = k & 15;
    r->unk228 = (e >> 15) & ((1 << len) - 1);
    r->filter_file = e >> (15 + len);
    return r;
}
#define XA_CONFIG(i) (*ModXaConfig(i))
