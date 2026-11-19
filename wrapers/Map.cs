using System.Collections.Generic;
using RecompOne.Runtime.Memory;

namespace Sotn;

public static class Map
{
    public const uint CastleMapAddr = 0x8006BB74u;
    public const int Length = 0x800;
    public const int Width = 64;
    /// <summary>Bytes per map row: 4 cells per byte, 2 bits each, the cell with x % 4 == 0 in the top bits
    /// (seen in game: entering cell (3,40) set bits 0-1 of byte 40*16 + 3/4).</summary>
    public const int RowBytes = 16;
    public const byte Visited = 0xFF;

    static IMemory M => RecompOne.Runtime.Runtime.Mem!;

    public static byte GetRoom(int index) => M.ReadU8(CastleMapAddr + (uint)index);
    public static void SetRoom(int index, byte value) => M.WriteU8(CastleMapAddr + (uint)index, value);

    static int CellShift(int x) => (3 - (x & 3)) * 2;

    /// <summary>The 2-bit state of map cell (x, y); 0 = not visited.</summary>
    public static byte GetRoom(int x, int y)
    {
        int i = y * RowBytes + (x >> 2);
        if ((uint)x >= Width || (uint)i >= Length) return 0;
        return (byte)((GetRoom(i) >> CellShift(x)) & 3);
    }

    public static void SetRoom(int x, int y, byte value)
    {
        int i = y * RowBytes + (x >> 2);
        if ((uint)x >= Width || (uint)i >= Length) return;
        int shift = CellShift(x);
        SetRoom(i, (byte)((GetRoom(i) & ~(3 << shift)) | ((value & 3) << shift)));
    }

    public static bool IsVisited(int index) => GetRoom(index) != 0;
    public static void SetVisited(int index) => SetRoom(index, Visited);
    public static void SetUnvisited(int index) => SetRoom(index, 0);

    public static void RevealAll()
    {
        for (int i = 0; i < Length; i++) SetRoom(i, Visited);
    }

    public static void HideAll()
    {
        for (int i = 0; i < Length; i++) SetRoom(i, 0);
    }

    public static IEnumerable<int> VisitedRooms()
    {
        for (int i = 0; i < Length; i++)
            if (GetRoom(i) != 0) yield return i;
    }

    public static int VisitedCount
    {
        get
        {
            // visited cells, not bytes: each byte holds 4 cells
            int n = 0;
            for (int i = 0; i < Length; i++)
            {
                byte b = GetRoom(i);
                for (int k = 0; k < 4; k++)
                    if (((b >> (k * 2)) & 3) != 0) n++;
            }
            return n;
        }
    }
}
