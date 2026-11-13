namespace RecompOne.Runtime.Config;

public class ViewConfig
{
    public bool GetBool(string key, bool fallback = false) => Settings.Get(key, fallback);
    public void SetBool(string key, bool value) => Settings.Set(key, value);

    public int GetInt(string key, int fallback = 0) => Settings.Get(key, fallback);
    public void SetInt(string key, int value) => Settings.Set(key, value);

    public float GetFloat(string key, float fallback = 0f) => Settings.Get(key, fallback);
    public void SetFloat(string key, float value) => Settings.Set(key, value);

    public string GetString(string key, string fallback = "") => Settings.Get(key, fallback);
    public void SetString(string key, string value) => Settings.Set(key, value);
}
