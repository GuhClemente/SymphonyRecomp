# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: PS1 VAB (VH header + VB sample body) reading and writing.

VH layout (little endian): a 32-byte header ("pBAV", version, id, size,
reserved, ps programs, ts tones, vs vags, master volume/pan, attributes),
128 program records of 16 bytes, then 16 tone records of 32 bytes for each
of the ps programs (in program order, programs with tones only), then 256
u16 VAG sizes (bytes >> 3; entry 0 unused). VB = the VAG bodies back to back.
"""
import struct

PROG_FMT = "<BBBBBBHII"  # tones, mvol, prior, mode, mpan, reserved0, attr, r1, r2
TONE_FIELDS = ("prior", "mode", "vol", "pan", "center", "shift", "min", "max",
               "vibW", "vibT", "porW", "porT", "pbmin", "pbmax", "r1", "r2",
               "adsr1", "adsr2", "prog", "vag", "r3", "r4", "r5", "r6")
TONE_FMT = "<BBBBBBBBBBBBBBBBHHhhhhhh"


class Vab:
    def __init__(self, vh, vb):
        self.header = bytearray(vh[:32])
        magic, version, vabid, fsize, r0, ps, ts, vs = struct.unpack_from("<IIIIHHHH", vh)
        assert magic == 0x56414270, hex(magic)
        self.programs = [list(struct.unpack_from(PROG_FMT, vh, 32 + i * 16)) for i in range(128)]
        self.tones = {}  # program -> list of 16 tone dicts
        off = 32 + 128 * 16
        for p in range(128):
            if self.programs[p][0] == 0:
                continue
            tl = []
            for t in range(16):
                vals = struct.unpack_from(TONE_FMT, vh, off + t * 32)
                tl.append(dict(zip(TONE_FIELDS, vals)))
            self.tones[p] = tl
            off += 16 * 32
        assert len(self.tones) == ps, (len(self.tones), ps)
        sizes = struct.unpack_from("<256H", vh, off)
        self.vags = []
        pos = 0
        for i in range(1, vs + 1):
            n = sizes[i] << 3
            self.vags.append(bytes(vb[pos:pos + n]))
            pos += n
        assert len(vh) == off + 512, (len(vh), off + 512)

    def used_tones(self, p):
        return [t for t in self.tones.get(p, []) if t["vag"] > 0]

    def write(self):
        ps = len(self.tones)
        ts = sum(self.programs[p][0] for p in self.tones)
        vb = b"".join(self.vags)
        hdr = bytearray(self.header)
        size = 32 + 128 * 16 + ps * 512 + 512 + len(vb)
        struct.pack_into("<IHHH", hdr, 12, size, struct.unpack_from("<H", hdr, 16)[0], ps, ts)
        struct.pack_into("<H", hdr, 22, len(self.vags))
        out = bytearray(hdr)
        for p in range(128):
            out += struct.pack(PROG_FMT, *self.programs[p])
        for p in sorted(self.tones):
            for t in self.tones[p]:
                out += struct.pack(TONE_FMT, *(t[f] for f in TONE_FIELDS))
        sizes = [0] + [len(v) >> 3 for v in self.vags]
        sizes += [0] * (256 - len(sizes))
        out += struct.pack("<256H", *sizes)
        return bytes(out), vb


if __name__ == "__main__":
    import sys
    v = Vab(open(sys.argv[1], "rb").read(), open(sys.argv[2], "rb").read())
    print("header", v.header.hex())
    print("vags", len(v.vags), "bytes", sum(map(len, v.vags)))
    for p in sorted(v.tones):
        print("prog %d: %s" % (p, v.programs[p]))
        for i, t in enumerate(v.tones[p]):
            if t["vag"]:
                print("  tone %2d vag %2d (%5d B) center %d.%d keys %d-%d vol %d pan %d adsr %04X %04X prior %d mode %d" % (
                    i, t["vag"], len(v.vags[t["vag"] - 1]), t["center"], t["shift"], t["min"], t["max"],
                    t["vol"], t["pan"], t["adsr1"], t["adsr2"], t["prior"], t["mode"]))
