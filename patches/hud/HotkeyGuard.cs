using ImGuiNET;

namespace Recompiled;

public static class HotkeyGuard
{
    /// <summary>True while an ImGui text field has focus (chat, IP, player name), when single-letter
    /// hotkeys must not fire.</summary>
    public static bool IsTyping()
    {
        try { return ImGui.GetIO().WantTextInput; }
        catch { return false; }
    }
}
