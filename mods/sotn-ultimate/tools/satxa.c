// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: Saturn SOTN music (SD/SDxx.PCM) -> PS1 XA ADPCM sectors.
//
//   satxa enc in.PCM out.xa file channel gain [min_seconds [fade_ms]] -> one
//       channel's raw 2352-byte sectors (37800 Hz stereo 4-bit, mode 2 form
//       2, MSF left at 0) and, on stdout, "sectors N intro_sectors I
//       loop_seconds L intro_seconds S ...". fade_ms: the seek back's pause
//       at the loop's quietest stretch, after a fade-out of that length
//   satxa dec in.xa out.wav file channel   -> decode one channel of an XA
//       stream (raw 2352 sectors) to a WAV, for checking
//   satxa wav in.PCM out.wav               -> the Saturn file decoded, 44100 Hz
//   satxa encmono in.wav out.xa file channel -> a 37800 Hz WAV as 37.8 kHz
//       MONO XA sectors (PSP dub voices, mods/sotn-ultimate/tools/psp_dub.py)
//   satxa encstereo in.wav out.xa file channel gain -> a 37800 Hz stereo WAV
//       as 37.8 kHz stereo XA sectors (the PSP English fairy song,
//       mods/sotn-ultimate/tools/gen_saturn_music.py)
//
// Saturn format (GAME func_06013538): header "DVI." of 0x800 bytes, BE u32
// data size at +8 and loop start at +0xC (a byte offset = a sample index,
// 0xFFFFFFFF = no loop); IMA ADPCM, stereo interleaved every 4 bytes (L, R),
// high nibble first, one continuous state per channel, 44100 Hz.
//
// The output is resampled to 37800 Hz with a fractional lead-in of silence
// so that the loop start falls exactly on a sector boundary (2016 stereo
// samples a sector): the PS1 player (src/dra/sound.c) seeks back to the loop
// entry's sector. After the loop end, about a second of the loop start
// follows, so a late seek still plays the right music.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int ima_step[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int ima_idx[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static uint8_t* read_file(const char* path, long* size) {
    FILE* f = fopen(path, "rb");
    uint8_t* buf;
    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(*size);
    if (fread(buf, 1, *size, f) != (size_t)*size) {
        perror(path);
        exit(1);
    }
    fclose(f);
    return buf;
}

static uint32_t be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

// -> pcm[2][n], returns n; *loop = loop start sample or -1
static long saturn_decode(const char* path, int16_t** pcm, long* loop) {
    long size, n, i;
    uint8_t* raw = read_file(path, &size);
    uint32_t data_size;
    int ch, k;

    if (memcmp(raw, "DVI.", 4)) {
        fprintf(stderr, "%s: not a DVI file\n", path);
        exit(1);
    }
    data_size = be32(raw + 8);
    *loop = be32(raw + 12) == 0xFFFFFFFF ? -1 : (long)be32(raw + 12);
    if (0x800 + (long)data_size > size) {
        data_size = size - 0x800;
    }
    n = data_size / 8 * 8;
    pcm[0] = malloc(n * sizeof(int16_t));
    pcm[1] = malloc(n * sizeof(int16_t));
    for (ch = 0; ch < 2; ch++) {
        int p = 0, idx = 0;
        long o = 0;
        for (i = 0; i < (long)data_size / 8; i++) {
            const uint8_t* b = raw + 0x800 + i * 8 + ch * 4;
            for (k = 0; k < 8; k++) {
                int nib = k & 1 ? b[k >> 1] & 15 : b[k >> 1] >> 4;
                int step = ima_step[idx];
                int d = step >> 3;
                if (nib & 1)
                    d += step >> 2;
                if (nib & 2)
                    d += step >> 1;
                if (nib & 4)
                    d += step;
                p = nib & 8 ? p - d : p + d;
                p = p < -32768 ? -32768 : p > 32767 ? 32767 : p;
                idx += ima_idx[nib & 7];
                idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
                pcm[ch][o++] = p;
            }
        }
    }
    free(raw);
    return n;
}

static void write_wav(const char* path, int16_t** pcm, long n, int rate) {
    FILE* f = fopen(path, "wb");
    uint32_t v;
    long i;
#define W32(x) (v = (x), fwrite(&v, 4, 1, f))
#define W16(x) (fputc((x) & 0xFF, f), fputc(((x) >> 8) & 0xFF, f))
    fwrite("RIFF", 1, 4, f);
    W32(36 + n * 4);
    fwrite("WAVEfmt ", 1, 8, f);
    W32(16);
    W16(1);
    W16(2);
    W32(rate);
    W32(rate * 4);
    W16(4);
    W16(16);
    fwrite("data", 1, 4, f);
    W32(n * 4);
    for (i = 0; i < n; i++) {
        W16(pcm[0][i]);
        W16(pcm[1][i]);
    }
    fclose(f);
}

// ---- resampling 44100 -> 37800 (x 6/7), windowed sinc
#define TAPS 48
static double sinc(double x) { return x == 0 ? 1 : sin(M_PI * x) / (M_PI * x); }

// input sample t of the "virtual" track: the track, then the loop again
static double in_at(const int16_t* s, long n, long loop, long t) {
    if (t < 0) {
        return 0;
    }
    if (t >= n) {
        if (loop < 0) {
            return 0;
        }
        t = loop + (t - n) % (n - loop);
    }
    return s[t];
}

// ---- XA ADPCM
static const int k0[4] = {0, 60, 115, 98};
static const int k1[4] = {0, 0, -52, -55};

typedef struct {
    int old, older;
} XaState;

static int clamp16(int v) {
    return v < -32768 ? -32768 : v > 32767 ? 32767 : v;
}

// encodes 28 samples, returns the param byte; nibbles in out[28]
static int encode_unit(
    const int16_t* x, XaState* st, uint8_t* out, int only_filter0) {
    int best_f = 0, best_r = 0;
    double best_err = 1e300;
    int f, r, i;

    for (f = 0; f < (only_filter0 ? 1 : 4); f++) {
        for (r = 0; r <= 12; r++) {
            int o = st->old, oo = st->older;
            double err = 0;
            for (i = 0; i < 28; i++) {
                int pred = (o * k0[f] + oo * k1[f] + 32) >> 6;
                int res = x[i] - pred;
                // nibble t: decoded (t << 12) >> r
                double q = (double)res * (1 << r) / 4096.0;
                int t = (int)lround(q);
                int s;
                t = t < -8 ? -8 : t > 7 ? 7 : t;
                s = clamp16(((t << 12) >> r) + pred);
                err += (double)(s - x[i]) * (s - x[i]);
                if (err >= best_err) {
                    break;
                }
                oo = o;
                o = s;
            }
            if (err < best_err) {
                best_err = err;
                best_f = f;
                best_r = r;
            }
        }
    }
    for (i = 0; i < 28; i++) {
        int pred = (st->old * k0[best_f] + st->older * k1[best_f] + 32) >> 6;
        int res = x[i] - pred;
        int t = (int)lround((double)res * (1 << best_r) / 4096.0);
        int s;
        t = t < -8 ? -8 : t > 7 ? 7 : t;
        s = clamp16(((t << 12) >> best_r) + pred);
        out[i] = t & 15;
        st->older = st->old;
        st->old = s;
    }
    return best_f << 4 | best_r;
}

static void sector_header(
    uint8_t* sec, int file, int channel, int submode, int coding) {
    static const uint8_t sync[12] = {
        0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    memset(sec, 0, 2352);
    memcpy(sec, sync, 12);
    sec[15] = 2; // mode 2 (MSF: mods/sotn-ultimate/tools/fix_sector_headers.py)
    sec[16] = sec[20] = file;
    sec[17] = sec[21] = channel;
    sec[18] = sec[22] = submode;
    sec[19] = sec[23] = coding;
}

// the quiet-spot jump (enc's fade_ms): the stretch looked for, around the
// jump point, is the fade-out before it, the seek's pause (measured ~0.47 s
// in DuckStation) and as long again after it; the fade ends this many output
// samples (one 59.826 Hz VSync) before the jump, so a VSync-early timer
// still lands in silence
#define JUMP_GAP_MS 468
#define JUMP_EARLY 632

// the loop body's quietest stretch, a whole number of sectors (2352 input
// samples = 2016 output ones) into it, in sectors; integer sums, so the
// WebAssembly build picks the same one
static long quiet_sector(int16_t** pcm, long n, long loop, int fade_ms) {
    long len = n - loop, fade = 44100L * fade_ms / 1000;
    long gap = 44100L * JUMP_GAP_MS / 1000, j, best = 0, t;
    long long* cum = malloc((len + 1) * sizeof(long long));
    long long e, best_e = 0;
    cum[0] = 0;
    for (t = 0; t < len; t++) {
        long long l = pcm[0][loop + t], r = pcm[1][loop + t];
        cum[t + 1] = cum[t] + l * l + r * r;
    }
    for (j = (fade + 2351) / 2352; j * 2352 + gap + fade <= len; j++) {
        e = cum[j * 2352 + gap + fade] - cum[j * 2352 - fade];
        if (!best || e < best_e) {
            best = j;
            best_e = e;
        }
    }
    free(cum);
    return best;
}

static void cmd_enc(const char* in, const char* out, int file, int channel,
                    double gain, double min_seconds, int fade_ms) {
    int16_t* pcm[2];
    long loop, n = saturn_decode(in, pcm, &loop);
    double ratio = 44100.0 / 37800.0;        // input samples per output sample
    double cutoff = 0.9 * 37800.0 / 44100.0; // of the input Nyquist
    double lead;
    long intro_sectors, total_out, sectors, i, s;
    int16_t* res[2];
    XaState st[2] = {{0, 0}, {0, 0}};
    FILE* f;
    long loop_out;
    long reps = 1; // loop bodies in the data (the seek back comes after all)
    long quiet = 0; // the loop entry, in sectors into the loop body
    long entry;     // the loop entry's sector: the seek back's target

    // output sample m reads input time (m - lead) * ratio
    if (loop >= 0) {
        double l = loop / ratio;
        intro_sectors = (long)ceil(l / 2016.0);
        lead = intro_sectors * 2016.0 - l;
    } else {
        intro_sectors = 0;
        lead = 0;
    }
    // the PS1 player's seek back to the loop entry stops the stream for the
    // seek's time (as on retail tracks): repeat the loop body until the data
    // lasts min_seconds, so that gap comes every few minutes, not every loop
    while (loop >= 0 &&
           (lead + loop / ratio) / 37800.0 + reps * (n - loop) / 44100.0 <
               min_seconds) {
        reps++;
    }
    // fade_ms: the seek back's pause goes to the loop's quietest stretch
    // instead of its end. The loop entry moves that far into the loop body;
    // the data plays on through the next pass to the same point, faded out
    // there, then silence (the seek still comes once every reps loops)
    if (loop >= 0 && fade_ms > 0) {
        quiet = quiet_sector(pcm, n, loop, fade_ms);
    }
    entry = intro_sectors + quiet;
    // where the (last) loop end falls; in_at() continues the loop seamlessly
    loop_out = (long)ceil(
        lead + (n + (reps - 1) * (loop >= 0 ? n - loop : 0)) / ratio);
    loop_out += quiet * 2016; // the seek point (= the loop end without a jump)
    total_out = loop_out + (loop >= 0 ? 37800 : 0);
    sectors = (total_out + 2015) / 2016;
    total_out = sectors * 2016;
    res[0] = malloc(total_out * sizeof(int16_t));
    res[1] = malloc(total_out * sizeof(int16_t));
    // (i - lead) * 7/6 has the same fraction for every i of the same i % 6
    static double kern[6][2 * TAPS];
    for (i = 0; i < 6; i++) {
        double t = (i - lead) * ratio;
        double frac = t - floor(t), wsum = 0;
        int j;
        for (j = 0; j < 2 * TAPS; j++) {
            double d = frac + TAPS - 1 - j; // t - (c - TAPS + 1 + j)
            double w = 0.5 + 0.5 * cos(M_PI * d / TAPS); // Hann
            kern[i][j] = cutoff * sinc(cutoff * d) * w;
            wsum += kern[i][j];
        }
        for (j = 0; j < 2 * TAPS; j++) {
            kern[i][j] /= wsum;
        }
    }
    for (i = 0; i < total_out; i++) {
        double t = (i - lead) * ratio;
        long c = (long)floor(t);
        const double* kk = kern[i % 6];
        int ch;
        for (ch = 0; ch < 2; ch++) {
            double acc = 0;
            int j;
            if (loop < 0 && t >= n) {
                res[ch][i] = 0;
                continue;
            }
            for (j = 0; j < 2 * TAPS; j++) {
                acc += kk[j] * in_at(pcm[ch], n, loop, c - TAPS + 1 + j);
            }
            res[ch][i] = clamp16((int)lround(acc * gain));
        }
    }
    if (quiet) {
        // linear fade-out ending JUMP_EARLY before the seek point, silence
        // after (integer math: the WebAssembly build matches)
        long fade = 37800L * fade_ms / 1000, end = loop_out - JUMP_EARLY;
        int ch;
        for (i = end - fade; i < total_out; i++) {
            for (ch = 0; ch < 2; ch++) {
                res[ch][i] = i < end ? (int)((long long)res[ch][i] *
                                             (end - i) / fade)
                                     : 0;
            }
        }
    }
    f = fopen(out, "wb");
    for (s = 0; s < sectors; s++) {
        uint8_t sec[2352];
        int g;
        sector_header(sec, file, channel, 0x64, 0x01);
        for (g = 0; g < 18; g++) {
            uint8_t* grp = sec + 24 + g * 128;
            long base = s * 2016 + g * 112;
            int u;
            for (u = 0; u < 8; u++) {
                int16_t x[28];
                uint8_t nib[28];
                int ch = u & 1, k, param;
                // the start and the loop sector start without history
                int f0 = g == 0 && (s == 0 || s == entry);
                for (k = 0; k < 28; k++) {
                    x[k] = res[ch][base + (u >> 1) * 28 + k];
                }
                param = encode_unit(x, &st[ch], nib, f0);
                grp[4 + u] = param;
                for (k = 0; k < 28; k++) {
                    grp[16 + k * 4 + (u >> 1)] |= nib[k] << ((u & 1) * 4);
                }
            }
            memcpy(grp, grp + 4, 4);
            memcpy(grp + 12, grp + 8, 4);
        }
        fwrite(sec, 1, 2352, f);
    }
    fclose(f);
    // intro_sectors / intro_seconds: up to the loop entry (with a jump, the
    // intro config plays on into the loop body to it, no seek)
    printf("sectors %ld intro_sectors %ld intro_seconds %.6f loop_seconds %.6f "
           "lead %.3f repeats %ld quiet_sectors %ld\n",
           sectors, entry, entry * 2016 / 37800.0,
           loop >= 0 ? (n - loop) / 44100.0 : n / 44100.0, lead, reps, quiet);
    // (the builder runs every command in one WebAssembly memory)
    free(pcm[0]);
    free(pcm[1]);
    free(res[0]);
    free(res[1]);
}

static void cmd_dec(const char* in, const char* out, int file, int channel) {
    long size, n = 0, s;
    uint8_t* raw = read_file(in, &size);
    int16_t* pcm[2];
    int old[2] = {0, 0}, older[2] = {0, 0};

    pcm[0] = malloc(size / 2352 * 2016 * sizeof(int16_t));
    pcm[1] = malloc(size / 2352 * 2016 * sizeof(int16_t));
    for (s = 0; s < size / 2352; s++) {
        const uint8_t* sec = raw + s * 2352;
        int g;
        if (sec[16] != file || sec[17] != channel || !(sec[18] & 4)) {
            continue;
        }
        if (!(sec[19] & 1)) { // mono: 8 units of consecutive samples
            pcm[0] = realloc(pcm[0], (n + 4032 * (size / 2352 - s + 1)) * 2);
            pcm[1] = realloc(pcm[1], (n + 4032 * (size / 2352 - s + 1)) * 2);
            for (g = 0; g < 18; g++) {
                const uint8_t* grp = sec + 24 + g * 128;
                int u, k;
                for (u = 0; u < 8; u++) {
                    int f = grp[4 + u] >> 4 & 3, r = grp[4 + u] & 15;
                    if (r > 12)
                        r = 9;
                    for (k = 0; k < 28; k++) {
                        int t =
                            grp[16 + k * 4 + (u >> 1)] >> ((u & 1) * 4) & 15;
                        int v;
                        t = t >= 8 ? t - 16 : t;
                        v = clamp16(
                            ((t << 12) >> r) +
                            ((old[0] * k0[f] + older[0] * k1[f] + 32) >> 6));
                        older[0] = old[0];
                        old[0] = v;
                        pcm[0][n + g * 224 + u * 28 + k] = v;
                        pcm[1][n + g * 224 + u * 28 + k] = v;
                    }
                }
            }
            n += 4032;
            continue;
        }
        for (g = 0; g < 18; g++) {
            const uint8_t* grp = sec + 24 + g * 128;
            int u, k;
            for (u = 0; u < 8; u++) {
                int ch = u & 1, f = grp[4 + u] >> 4 & 3, r = grp[4 + u] & 15;
                if (r > 12)
                    r = 9;
                for (k = 0; k < 28; k++) {
                    int t = grp[16 + k * 4 + (u >> 1)] >> ((u & 1) * 4) & 15;
                    int v;
                    t = t >= 8 ? t - 16 : t;
                    v = clamp16(
                        ((t << 12) >> r) +
                        ((old[ch] * k0[f] + older[ch] * k1[f] + 32) >> 6));
                    older[ch] = old[ch];
                    old[ch] = v;
                    pcm[ch][n + g * 112 + (u >> 1) * 28 + k] = v;
                }
            }
        }
        n += 2016;
    }
    write_wav(out, pcm, n, 37800);
    printf("%ld samples (%.2f s)\n", n, n / 37800.0);
    free(pcm[0]);
    free(pcm[1]);
    free(raw);
}

// ---- SPU ADPCM (VAB sample bodies): 28 samples in 16-byte blocks, filters
// 0-4, laid out as the retail banks: a zero block, the data (first block flag
// 4, last flag 1), then a silent terminator block (flag 7)
static const int spu_k0[5] = {0, 60, 115, 98, 122};
static const int spu_k1[5] = {0, 0, -52, -55, -60};

static void spu_block(const int16_t* x, XaState* st, uint8_t* out) {
    int best_f = 0, best_r = 0, f, r, i;
    double best_err = 1e300;

    for (f = 0; f < 5; f++) {
        for (r = 0; r <= 12; r++) {
            int o = st->old, oo = st->older;
            double err = 0;
            for (i = 0; i < 28 && err < best_err; i++) {
                int pred = (o * spu_k0[f] + oo * spu_k1[f] + 32) >> 6;
                int t = (int)lround((double)(x[i] - pred) * (1 << r) / 4096.0);
                int s;
                t = t < -8 ? -8 : t > 7 ? 7 : t;
                s = clamp16(((t << 12) >> r) + pred);
                err += (double)(s - x[i]) * (s - x[i]);
                oo = o;
                o = s;
            }
            if (err < best_err) {
                best_err = err;
                best_f = f;
                best_r = r;
            }
        }
    }
    memset(out, 0, 16);
    out[0] = best_f << 4 | best_r; // decoded (t << 12) >> shift, as XA
    for (i = 0; i < 28; i++) {
        int pred =
            (st->old * spu_k0[best_f] + st->older * spu_k1[best_f] + 32) >> 6;
        int t = (int)lround((double)(x[i] - pred) * (1 << best_r) / 4096.0);
        int s;
        t = t < -8 ? -8 : t > 7 ? 7 : t;
        s = clamp16(((t << 12) >> best_r) + pred);
        out[2 + i / 2] |= (t & 15) << ((i & 1) * 4);
        st->older = st->old;
        st->old = s;
    }
}

// mono 16-bit WAV (stereo is mixed down) -> VAB sample body
static void cmd_vag(const char* in, const char* out) {
    long size, n, i, data = 12, len = 0;
    uint8_t* raw = read_file(in, &size);
    int chans = 1;
    int16_t* pcm;
    uint8_t blk[16];
    XaState st = {0, 0};
    FILE* f;

    while (data + 8 <= size) {
        uint32_t clen = raw[data + 4] | raw[data + 5] << 8 |
                        raw[data + 6] << 16 | (uint32_t)raw[data + 7] << 24;
        if (!memcmp(raw + data, "fmt ", 4)) {
            chans = raw[data + 10];
        } else if (!memcmp(raw + data, "data", 4)) {
            len = clen;
            data += 8;
            break;
        }
        data += 8 + clen;
    }
    n = len / 2 / chans;
    pcm = calloc(n + 28, sizeof(int16_t));
    for (i = 0; i < n; i++) {
        int c, acc = 0;
        for (c = 0; c < chans; c++) {
            const uint8_t* p = raw + data + (i * chans + c) * 2;
            acc += (int16_t)(p[0] | p[1] << 8);
        }
        pcm[i] = acc / chans;
    }
    f = fopen(out, "wb");
    memset(blk, 0, 16);
    fwrite(blk, 1, 16, f);
    for (i = 0; i < n; i += 28) {
        spu_block(pcm + i, &st, blk);
        blk[1] = i == 0 ? 4 : 0;
        if (i + 28 >= n) {
            blk[1] |= 1;
        }
        fwrite(blk, 1, 16, f);
    }
    memset(blk, 0x77, 16);
    blk[0] = 0;
    blk[1] = 7;
    fwrite(blk, 1, 16, f);
    fclose(f);
    free(raw);
    printf("%ld samples, %ld bytes\n", n, ((n + 27) / 28 + 2) * 16);
}

// 16-bit WAV, already 37800 Hz (stereo is mixed down) -> 37.8 kHz MONO 4-bit
// XA sectors (coding 0x00, submode 0x64, no EOF; 4032 samples a sector, the
// last one padded with silence). The PS1 player reads mono from every 16th
// sector (mods/sotn-ultimate/tools/repack_xa.py places it). Prints "sectors N
// samples M".
static void cmd_encmono(
    const char* in, const char* out, int file, int channel) {
    long size, n, i, data = 12, len = 0, sectors, s;
    uint8_t* raw = read_file(in, &size);
    int chans = 1, rate = 0;
    int16_t* pcm;
    XaState st = {0, 0};
    FILE* f;

    while (data + 8 <= size) {
        uint32_t clen = raw[data + 4] | raw[data + 5] << 8 |
                        raw[data + 6] << 16 | (uint32_t)raw[data + 7] << 24;
        if (!memcmp(raw + data, "fmt ", 4)) {
            chans = raw[data + 10];
            rate = raw[data + 12] | raw[data + 13] << 8 | raw[data + 14] << 16;
        } else if (!memcmp(raw + data, "data", 4)) {
            len = clen;
            data += 8;
            break;
        }
        data += 8 + clen;
    }
    if (rate != 37800) {
        fprintf(stderr, "%s: %d Hz, want 37800\n", in, rate);
        exit(1);
    }
    n = len / 2 / chans;
    sectors = (n + 4031) / 4032;
    pcm = calloc(sectors * 4032, sizeof(int16_t));
    for (i = 0; i < n; i++) {
        int c, acc = 0;
        for (c = 0; c < chans; c++) {
            const uint8_t* p = raw + data + (i * chans + c) * 2;
            acc += (int16_t)(p[0] | p[1] << 8);
        }
        pcm[i] = acc / chans;
    }
    f = fopen(out, "wb");
    for (s = 0; s < sectors; s++) {
        uint8_t sec[2352];
        int g;
        sector_header(sec, file, channel, 0x64, 0x00);
        for (g = 0; g < 18; g++) {
            uint8_t* grp = sec + 24 + g * 128;
            int u;
            for (u = 0; u < 8; u++) {
                uint8_t nib[28];
                int k, param;
                // a mono group: 8 units of 28 consecutive samples
                param = encode_unit(pcm + s * 4032 + g * 224 + u * 28, &st, nib,
                                    s == 0 && g == 0);
                grp[4 + u] = param;
                for (k = 0; k < 28; k++) {
                    grp[16 + k * 4 + (u >> 1)] |= nib[k] << ((u & 1) * 4);
                }
            }
            memcpy(grp, grp + 4, 4);
            memcpy(grp + 12, grp + 8, 4);
        }
        fwrite(sec, 1, 2352, f);
    }
    fclose(f);
    free(raw);
    printf("sectors %ld samples %ld\n", sectors, n);
}

// 16-bit stereo WAV, already 37800 Hz -> 37.8 kHz STEREO 4-bit XA sectors
// (coding 0x01, submode 0x64, no EOF; 2016 samples a sector, the last one
// padded with silence), scaled by gain. Prints "sectors N samples M".
static void cmd_encstereo(
    const char* in, const char* out, int file, int channel, double gain) {
    long size, n, i, data = 12, len = 0, sectors, s;
    uint8_t* raw = read_file(in, &size);
    int chans = 0, rate = 0;
    int16_t* res[2];
    XaState st[2] = {{0, 0}, {0, 0}};
    FILE* f;

    while (data + 8 <= size) {
        uint32_t clen = raw[data + 4] | raw[data + 5] << 8 |
                        raw[data + 6] << 16 | (uint32_t)raw[data + 7] << 24;
        if (!memcmp(raw + data, "fmt ", 4)) {
            chans = raw[data + 10];
            rate = raw[data + 12] | raw[data + 13] << 8 | raw[data + 14] << 16;
        } else if (!memcmp(raw + data, "data", 4)) {
            len = clen;
            data += 8;
            break;
        }
        data += 8 + clen;
    }
    if (rate != 37800 || chans != 2) {
        fprintf(stderr, "%s: %d Hz, %d channels, want 37800 stereo\n", in, rate,
                chans);
        exit(1);
    }
    n = len / 4;
    sectors = (n + 2015) / 2016;
    res[0] = calloc(sectors * 2016, sizeof(int16_t));
    res[1] = calloc(sectors * 2016, sizeof(int16_t));
    for (i = 0; i < n * 2; i++) {
        const uint8_t* p = raw + data + i * 2;
        res[i & 1][i >> 1] =
            clamp16((int)lround((int16_t)(p[0] | p[1] << 8) * gain));
    }
    f = fopen(out, "wb");
    for (s = 0; s < sectors; s++) {
        uint8_t sec[2352];
        int g;
        sector_header(sec, file, channel, 0x64, 0x01);
        for (g = 0; g < 18; g++) {
            uint8_t* grp = sec + 24 + g * 128;
            long base = s * 2016 + g * 112;
            int u;
            for (u = 0; u < 8; u++) {
                uint8_t nib[28];
                int ch = u & 1, k, param;
                param = encode_unit(res[ch] + base + (u >> 1) * 28, &st[ch],
                                    nib, s == 0 && g == 0);
                grp[4 + u] = param;
                for (k = 0; k < 28; k++) {
                    grp[16 + k * 4 + (u >> 1)] |= nib[k] << ((u & 1) * 4);
                }
            }
            memcpy(grp, grp + 4, 4);
            memcpy(grp + 12, grp + 8, 4);
        }
        fwrite(sec, 1, 2352, f);
    }
    fclose(f);
    free(raw);
    printf("sectors %ld samples %ld\n", sectors, n);
}

int main(int argc, char** argv) {
    if (argc == 7 && !strcmp(argv[1], "encstereo")) {
        cmd_encstereo(
            argv[2], argv[3], atoi(argv[4]), atoi(argv[5]), atof(argv[6]));
        return 0;
    }
    if (argc == 6 && !strcmp(argv[1], "encmono")) {
        cmd_encmono(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]));
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "vag")) {
        cmd_vag(argv[2], argv[3]);
        return 0;
    }
    if (argc >= 7 && argc <= 9 && !strcmp(argv[1], "enc")) {
        cmd_enc(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]), atof(argv[6]),
                argc >= 8 ? atof(argv[7]) : 0, argc == 9 ? atoi(argv[8]) : 0);
    } else if (argc == 6 && !strcmp(argv[1], "dec")) {
        cmd_dec(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]));
    } else if (argc == 4 && !strcmp(argv[1], "wav")) {
        int16_t* pcm[2];
        long loop, n = saturn_decode(argv[2], pcm, &loop);
        write_wav(argv[3], pcm, n, 44100);
        printf("%ld samples, loop %ld\n", n, loop);
    } else {
        fprintf(stderr, "usage: see the top of satxa.c\n");
        return 1;
    }
    return 0;
}
