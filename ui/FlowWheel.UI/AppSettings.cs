using System.Text.Json;
using System.Text.Json.Serialization;

namespace FlowWheel;

public sealed class AppSettings
{
    public const string NaturalPreset = "Natural";
    public const string GentlePreset = "Gentle";
    public const string FastPreset = "Fast";

    public bool Enabled { get; set; } = true;
    // Natural is intentionally conservative: one physical wheel notch stays close to the
    // Windows default while the cubic settle removes the abrupt stop.
    public string Preset { get; set; } = NaturalPreset;
    public int Sensitivity { get; set; } = 50;
    public int StepSize { get; set; } = 50;
    public int Duration { get; set; } = 260;
    public string Easing { get; set; } = "Cubic";
    // The neutral preset keeps acceleration off, matching the inspected SmoothScroll default
    // and avoiding surprising distance changes during ordinary document reading.
    public int Acceleration { get; set; } = 0;
    // Delta is measured in native wheel units and is used by the optional faster preset.
    public int AccelerationDelta { get; set; } = 75;
    public int MaxAcceleration { get; set; } = 100;
    public bool InvertDirection { get; set; }
    public bool SmoothHorizontal { get; set; } = true;
    public bool ShiftHorizontal { get; set; } = true;
    public string ExcludedApps { get; set; } = "";
    /// <summary>
    /// Absolute executable paths disabled from the tray's per-app command. Paths are used
    /// whenever available so duplicate process names cannot collide.
    /// </summary>
    public List<string> ExcludedAppPaths { get; set; } = new();
    /// <summary>Automatically pass through conservative game detections.</summary>
    public bool AutoDisableGames { get; set; } = true;
    /// <summary>Names or paths that should keep smoothing even when auto game bypass matches.</summary>
    public string GameBypassExceptions { get; set; } = "";
    public bool StartWithWindows { get; set; }
    public bool ShowNotifications { get; set; } = true;

    [JsonIgnore]
    public string DisplayProfile => Sensitivity >= 70 ? "Fast" : Sensitivity <= 30 ? "Gentle" : "Balanced";

    public static AppSettings CreateDefaults() => new();

    public void ResetToDefaults()
    {
        var defaults = CreateDefaults();
        Enabled = defaults.Enabled;
        Preset = defaults.Preset;
        Sensitivity = defaults.Sensitivity;
        StepSize = defaults.StepSize;
        Duration = defaults.Duration;
        Easing = defaults.Easing;
        Acceleration = defaults.Acceleration;
        AccelerationDelta = defaults.AccelerationDelta;
        MaxAcceleration = defaults.MaxAcceleration;
        InvertDirection = defaults.InvertDirection;
        SmoothHorizontal = defaults.SmoothHorizontal;
        ShiftHorizontal = defaults.ShiftHorizontal;
        ExcludedApps = defaults.ExcludedApps;
        ExcludedAppPaths = new List<string>(defaults.ExcludedAppPaths);
        AutoDisableGames = defaults.AutoDisableGames;
        GameBypassExceptions = defaults.GameBypassExceptions;
        StartWithWindows = defaults.StartWithWindows;
        ShowNotifications = defaults.ShowNotifications;
    }

    public void ApplyPreset(string preset)
    {
        Preset = preset switch
        {
            GentlePreset => GentlePreset,
            FastPreset => FastPreset,
            _ => NaturalPreset
        };

        switch (Preset)
        {
            case GentlePreset:
                Sensitivity = 42; StepSize = 42; Duration = 340; Acceleration = 0; AccelerationDelta = 85; MaxAcceleration = 70; Easing = "Smooth";
                break;
            case FastPreset:
                Sensitivity = 62; StepSize = 62; Duration = 180; Acceleration = 35; AccelerationDelta = 70; MaxAcceleration = 100; Easing = "Cubic";
                break;
            default:
                Sensitivity = 50; StepSize = 50; Duration = 260; Acceleration = 0; AccelerationDelta = 75; MaxAcceleration = 100; Easing = "Cubic";
                break;
        }
    }
}

public static class SettingsStore
{
    private static readonly JsonSerializerOptions Options = new() { WriteIndented = true };
    public static string DirectoryPath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "FlowWheel");
    public static string FilePath => Path.Combine(DirectoryPath, "options.json");

    public static AppSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
            {
                var loaded = JsonSerializer.Deserialize<AppSettings>(File.ReadAllText(FilePath), Options) ?? new AppSettings();
                loaded.ExcludedAppPaths ??= new List<string>();
                return loaded;
            }
        }
        catch { /* A damaged preferences file should never prevent the tray app from starting. */ }
        return new AppSettings();
    }

    public static void Save(AppSettings settings)
    {
        Directory.CreateDirectory(DirectoryPath);
        var temporary = FilePath + ".tmp";
        File.WriteAllText(temporary, JsonSerializer.Serialize(settings, Options));
        File.Move(temporary, FilePath, true);
    }
}
