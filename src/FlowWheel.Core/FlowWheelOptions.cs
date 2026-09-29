namespace FlowWheel.Core;

/// <summary>Runtime settings for the FlowWheel global scroll engine.</summary>
public sealed record FlowWheelOptions
{
    /// <summary>Enables interception and smoothing. This is separate from hook lifetime.</summary>
    public bool Enabled { get; init; } = true;

    /// <summary>Amount of native wheel delta represented by one input event.</summary>
    public double StepSize { get; init; } = 120d;

    /// <summary>Animation duration in milliseconds. Values outside 20..2000 are clamped.</summary>
    public double AnimationTime { get; init; } = 360d;

    /// <summary>Use the pulse/ease-out curve rather than a linear curve.</summary>
    public bool PulseAlgorithm { get; init; } = true;

    /// <summary>Scales the pulse curve (1 is a conservative default).</summary>
    public double PulseScale { get; init; } = 3d;

    /// <summary>Normalizes pulse output; values below zero are treated as zero.</summary>
    public double PulseNormalize { get; init; } = 1d;

    public bool AccelerationDefault { get; init; }
    public double AccelerationDelta { get; init; } = 70d;
    public double AccelerationMax { get; init; } = 7d;
    public bool ReverseDirection { get; init; }
    public bool HorizontalShiftKey { get; init; } = true;
    public bool HorizontalSmoothing { get; init; } = true;

    /// <summary>When false, incoming wheel messages are passed through untouched.</summary>
    public bool GlobalEnabled { get; init; } = true;

    /// <summary>Case-insensitive absolute executable paths that should be passed through.</summary>
    public IReadOnlySet<string> ExcludedProcessPaths { get; init; } =
        new HashSet<string>(StringComparer.OrdinalIgnoreCase);

    /// <summary>Case-insensitive process image names (for example, "pdfeditor.exe").</summary>
    public IReadOnlySet<string> ExcludedProcessNames { get; init; } =
        new HashSet<string>(StringComparer.OrdinalIgnoreCase);

    /// <summary>When enabled, known game processes and game-library binaries pass through.</summary>
    public bool AutoDisableGames { get; init; } = true;

    /// <summary>Process names/paths explicitly allowed to keep smoothing during game bypass.</summary>
    public IReadOnlySet<string> GameBypassExceptions { get; init; } =
        new HashSet<string>(StringComparer.OrdinalIgnoreCase);

    internal int DurationMilliseconds => (int)Math.Clamp(Math.Round(AnimationTime), 20d, 2000d);
    internal double SafeStepSize => Math.Clamp(Math.Abs(StepSize), 0d, 4800d);
    internal double SafePulseScale => Math.Clamp(PulseScale, 0d, 12d);
    internal double SafePulseNormalize => Math.Clamp(PulseNormalize, 0d, 12d);
    internal double SafeAccelerationDelta => Math.Clamp(Math.Abs(AccelerationDelta), 0d, 2400d);
    internal double SafeAccelerationMax => Math.Clamp(Math.Abs(AccelerationMax), 0d, 64d);
}
