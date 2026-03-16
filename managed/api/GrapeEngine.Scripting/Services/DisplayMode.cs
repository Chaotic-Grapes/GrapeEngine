namespace GrapeEngine.Scripting.Services;

/// <summary>
/// Display mode data (resolution + refresh rate).
/// </summary>
public readonly struct DisplayMode
{
    public readonly int Width;
    public readonly int Height;
    public readonly int RefreshRate;
    public readonly int BitsPerPixel;

    public DisplayMode(int width, int height, int refreshRate, int bitsPerPixel)
    {
        Width = width;
        Height = height;
        RefreshRate = refreshRate;
        BitsPerPixel = bitsPerPixel;
    }
}
