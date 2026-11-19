namespace RecompOne.Runtime.Events;

/// <summary>fires for each primitive before it is drawns</summary>
public sealed class RenderPrimEvent : GameEvent
{
    public int Count;
    public readonly int[] X = new int[4];
    public readonly int[] Y = new int[4];
    /// <summary>Texture coordinates per vertex (textured prims; for a rectangle U[0]/V[0] is its top-left).</summary>
    public readonly int[] U = new int[4];
    public readonly int[] V = new int[4];
    /// <summary>Texture page origin in VRAM and depth (0 = 4-bit, 1 = 8-bit, 2 = 15-bit) the prim samples from.</summary>
    public int TexPageX, TexPageY, TexDepth;
    public int DrawLeft, DrawRight, DrawTop, DrawBottom;
    public bool Textured, SemiTransparent, Gouraud, Raw;
    public int Clut, TexPage;
    public bool Skip;
}