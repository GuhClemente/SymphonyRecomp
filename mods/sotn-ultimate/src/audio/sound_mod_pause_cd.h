// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/sound.c (at file scope, in place)
// Saturn port: on the Saturn, pausing as Richter or Maria (Start, no menu)
// pauses the music, and it goes on from there when unpaused. Here the XA
// stream's read pauses (CdlPause) and resumes where it stopped (CdlReadN
// with no Setloc); while paused the music timer's start (D_8013AEF4) is
// moved MOD_CD_PAUSED ahead so func_80133FCC never sees the track end, and
// on resume by the paused time, so the intro / loop still end on time.
#define MOD_CD_PAUSED 0x40000000
void ModPauseCd(s32 resume) {
    if (resume) {
        if (D_8013AEF4 - VSync(-1) >= MOD_CD_PAUSED / 2) {
            CdControlF(CdlReadN, NULL);
            D_8013AEF4 += VSync(-1) - MOD_CD_PAUSED;
        }
    } else if (g_PlayingXaBgmId && !D_801390A0) {
        CdControlF(CdlPause, NULL);
        D_8013AEF4 += MOD_CD_PAUSED - VSync(-1);
    }
}
