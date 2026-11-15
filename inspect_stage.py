import os, sys, struct

class IsoFs:
    def __init__(self, bin_path):
        self.f = open(bin_path, 'rb')
    def read_sector(self, lba):
        self.f.seek(lba * 2352 + 24)
        return self.f.read(2048)
    def read_file(self, lba, size):
        sectors = (size + 2047) // 2048
        buf = bytearray()
        for i in range(sectors):
            buf.extend(self.read_sector(lba + i))
        return bytes(buf[:size])
    def find_file(self, path):
        pvd = self.read_sector(16)
        root_rec = pvd[156:156+34]
        root_lba = int.from_bytes(root_rec[2:6], 'little')
        root_size = int.from_bytes(root_rec[10:14], 'little')
        parts = path.strip('/').upper().split('/')
        cur_lba, cur_size = root_lba, root_size
        for part in parts:
            found = False
            dir_data = self.read_file(cur_lba, cur_size)
            offset = 0
            while offset < len(dir_data):
                rec_len = dir_data[offset]
                if rec_len == 0:
                    offset = (offset // 2048 + 1) * 2048
                    continue
                rec = dir_data[offset:offset+rec_len]
                name_len = rec[32]
                name = rec[33:33+name_len].decode('ascii', errors='ignore').split(';')[0]
                lba = int.from_bytes(rec[2:6], 'little')
                size = int.from_bytes(rec[10:14], 'little')
                if name == part:
                    cur_lba, cur_size = lba, size
                    found = True
                    break
                offset += rec_len
            if not found:
                return None
        return self.read_file(cur_lba, cur_size)

iso = IsoFs('disc/Castlevania - Symphony of the Night (Track 1).bin')

def get_rooms(bin_name):
    path = f'ST/{bin_name}/{bin_name}.BIN'
    data = iso.find_file(path)
    if not data or len(data) < 0x20:
        return []
    table_ptr = int.from_bytes(data[0x10:0x14], 'little')
    off = table_ptr - 0x80180000
    rooms = []
    while off + 8 <= len(data):
        l, t, r, b = data[off], data[off+1], data[off+2], data[off+3]
        if l == 0x40: break
        ptr = int.from_bytes(data[off+4:off+8], 'little')
        rooms.append({'left': l, 'top': t, 'right': r, 'bottom': b,
                      'w': r - l + 1, 'h': b - t + 1, 'ptr': ptr, 'ptr_hex': hex(ptr)})
        off += 8
    return rooms

if __name__ == '__main__':
    stage = sys.argv[1] if len(sys.argv) > 1 else 'NP3'
    rooms = get_rooms(stage)
    # ptr packs tileLayoutId, tilesetId, objGfxId, objLayoutId; tileset 0xFF is an exit to another stage,
    # and a 1x1 room beside one is the CD loading corridor (the game walks the player through it)
    exits = [r for r in rooms if (r['ptr'] >> 8) & 0xFF == 0xFF]
    print(f'=== Stage {stage} ({len(rooms)} rooms) ===')
    for i, r in enumerate(rooms):
        is_exit = (r['ptr'] >> 8) & 0xFF == 0xFF
        single = r['w'] == 1 and r['h'] == 1
        corridor = single and not is_exit and any(abs(r['left'] - e['left']) == 1 and r['top'] == e['top'] for e in exits)
        note = 'saida de estagio' if is_exit else 'corredor de CD' if corridor else '1x1' if single else ''
        print(f"  [{i:2d}] ({r['left']:2d},{r['top']:2d}) to ({r['right']:2d},{r['bottom']:2d}) [w={r['w']}, h={r['h']}] "
              f"layout=0x{r['ptr'] & 0xFF:02x} objLayout=0x{r['ptr'] >> 24:02x}  {note}")
