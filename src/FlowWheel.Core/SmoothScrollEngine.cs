using System.Diagnostics;
using FlowWheel.Core.Interop;

namespace FlowWheel.Core;

/// <summary>
/// Converts wheel input into timestamped, eased wheel pulses. The low-level hook only queues
/// work and returns immediately; all SendInput calls run on the animator thread.
/// </summary>
public sealed class SmoothScrollEngine : IDisposable
{
    private readonly object _gate = new();
    private readonly GlobalMouseHook _hook;
    private readonly ProcessExclusionMatcher _exclusions = new();
    private readonly AutoResetEvent _wake = new(false);
    private readonly List<ScrollSegment> _segments = new();
    private Thread? _animationThread;
    private FlowWheelOptions _options;
    private bool _running;
    private bool _stopping;
    // If SendInput fails (for example because the foreground target is elevated), keep the
    // hook fail-open.  A dead animator must never continue returning TRUE from the hook and
    // swallow the user's physical wheel input.
    private bool _animationFaulted;
    private double _verticalCarry;
    private double _horizontalCarry;
    private int _lastDirection;
    private long _lastWheelTimestamp;
    private double _acceleration = 1d;

    public SmoothScrollEngine(FlowWheelOptions? options = null, GlobalMouseHook? hook = null)
    {
        _options = options ?? new FlowWheelOptions();
        _hook = hook ?? new GlobalMouseHook();
        _hook.SetProcessor(HandleWheel);
        _hook.Faulted += exception => Faulted?.Invoke(exception);
    }

    public event Action<Exception>? Faulted;

    public FlowWheelOptions Options
    {
        get { lock (_gate) return _options; }
    }

    public bool IsRunning
    {
        get { lock (_gate) return _running && !_stopping; }
    }

    public void UpdateOptions(FlowWheelOptions options)
    {
        ArgumentNullException.ThrowIfNull(options);
        lock (_gate) _options = options;
        _wake.Set();
    }

    public void Start()
    {
        lock (_gate)
        {
            if (_running) return;
            _stopping = false;
            _animationFaulted = false;
            _animationThread = new Thread(AnimationThreadMain)
            {
                IsBackground = true,
                Name = "FlowWheel scroll animator"
            };
            _running = true;
            _animationThread.Start();
        }

        try
        {
            _hook.Start();
        }
        catch
        {
            Stop();
            throw;
        }
    }

    public void Stop()
    {
        Thread? animationThread;
        lock (_gate)
        {
            if (!_running) return;
            _stopping = true;
            _segments.Clear();
            animationThread = _animationThread;
        }

        // Stop receiving new input before joining the producer/consumer threads.
        _hook.Stop();
        _wake.Set();
        if (animationThread is not null && animationThread != Thread.CurrentThread)
            animationThread.Join(TimeSpan.FromSeconds(5));

        lock (_gate)
        {
            _running = false;
            _animationThread = null;
            _animationFaulted = false;
            _verticalCarry = 0;
            _horizontalCarry = 0;
        }
    }

    public void Dispose()
    {
        Stop();
        _hook.Dispose();
        _wake.Dispose();
    }

    private bool HandleWheel(MouseWheelEvent input)
    {
        FlowWheelOptions options;
        lock (_gate)
        {
            options = _options;
            if (!_running || _stopping || _animationFaulted || !options.Enabled || !options.GlobalEnabled)
                return false;
        }

        var exclusion = _exclusions.TryIsExcluded(
                input.TargetWindow,
                options.ExcludedProcessNames,
                options.ExcludedProcessPaths,
                options.AutoDisableGames,
                options.GameBypassExceptions);
        // Process identity is populated off the hook thread. Until it is known, leave the
        // physical event untouched; swallowing an unclassified first tick makes Explorer and
        // newly opened applications feel broken and could briefly defeat a game bypass.
        if (exclusion is null || exclusion.Value)
            return false;

        // A number of Explorer's native file-view controls do not accumulate the small
        // wheel deltas that are useful for browser content. Mark the segment so the animator
        // can commit one complete WHEEL_DELTA at the end of the easing interval.
        var explorerCompatibility = _exclusions.TryIsExplorer(input.TargetWindow) == true;

        var horizontal = input.Horizontal;
        if (!horizontal && input.ShiftDown && options.HorizontalShiftKey)
            horizontal = true;
        if (horizontal && !options.HorizontalSmoothing)
            return false;

        var direction = Math.Sign(input.Delta);
        if (direction == 0) return false;
        if (options.ReverseDirection) direction = -direction;

        var now = Stopwatch.GetTimestamp();
        var elapsed = _lastWheelTimestamp == 0
            ? double.PositiveInfinity
            : (now - _lastWheelTimestamp) * 1000d / Stopwatch.Frequency;
        if (!options.AccelerationDefault || direction != _lastDirection || elapsed > 220d)
            _acceleration = 1d;
        else if (options.SafeAccelerationDelta > 0d)
            _acceleration = Math.Min(options.SafeAccelerationMax, _acceleration + Math.Abs(input.Delta) / options.SafeAccelerationDelta);
        _lastWheelTimestamp = now;
        _lastDirection = direction;

        var magnitude = Math.Abs(input.Delta) * options.SafeStepSize / 120d * _acceleration;
        if (magnitude <= 0d) return true;
        var segment = new ScrollSegment(
            direction * magnitude,
            horizontal,
            now,
            options.DurationMilliseconds,
            explorerCompatibility);

        lock (_gate)
        {
            if (_stopping || !_running) return false;
            // A high-rate wheel can otherwise create an unbounded queue. Dropping the oldest
            // fully pending segments degrades gracefully while preserving latest user input.
            if (_segments.Count >= 256)
                _segments.RemoveRange(0, _segments.Count - 255);
            _segments.Add(segment);
        }
        _wake.Set();
        return true;
    }

    private void AnimationThreadMain()
    {
        try
        {
            while (true)
            {
                lock (_gate)
                {
                    if (_stopping) return;
                }

                Tick();
                _wake.WaitOne(8);
            }
        }
        catch (Exception exception)
        {
            lock (_gate)
            {
                // Leave the hook installed so settings/lifecycle remain responsive, but make
                // all subsequent input pass through until the engine is restarted. This is
                // especially important for UIPI failures when scrolling an elevated app.
                _animationFaulted = true;
                _segments.Clear();
                _verticalCarry = 0;
                _horizontalCarry = 0;
            }
            try { Faulted?.Invoke(exception); } catch { /* diagnostics must not stop cleanup */ }
        }
    }

    private void Tick()
    {
        var now = Stopwatch.GetTimestamp();
        var vertical = 0d;
        var horizontal = 0d;
        var explorerVertical = 0d;
        var explorerHorizontal = 0d;
        FlowWheelOptions options;

        lock (_gate)
        {
            options = _options;
            for (var index = _segments.Count - 1; index >= 0; index--)
            {
                var segment = _segments[index];
                var progress = Math.Clamp(
                    (now - segment.StartTimestamp) * 1000d / Stopwatch.Frequency / segment.DurationMilliseconds,
                    0d,
                    1d);
                if (segment.ExplorerCompatibility)
                {
                    // Legacy shell controls generally treat every SendInput call as a
                    // separate wheel message and discard values below WHEEL_DELTA. Emit a
                    // complete click once the eased segment settles. This keeps Explorer
                    // usable while the browser path retains fractional pulses.
                    if (progress >= 1d)
                    {
                        if (segment.Horizontal) explorerHorizontal += segment.Delta;
                        else explorerVertical += segment.Delta;
                        _segments.RemoveAt(index);
                    }
                    continue;
                }

                var eased = Ease(progress, options);
                var emitted = segment.Delta * eased;
                var increment = emitted - segment.Emitted;
                segment.Emitted = emitted;
                if (segment.Horizontal) horizontal += increment;
                else vertical += increment;
                if (progress >= 1d) _segments.RemoveAt(index);
            }
        }

        // Keep fractional wheel units between frames. Apps that accumulate WM_MOUSEWHEEL
        // deltas therefore receive a faithful total instead of rounded-away micro pulses.
        _verticalCarry += vertical;
        _horizontalCarry += horizontal;
        var verticalUnits = TakeWhole(ref _verticalCarry);
        var horizontalUnits = TakeWhole(ref _horizontalCarry);
        if (verticalUnits != 0)
            InjectWheel(verticalUnits, false);
        if (horizontalUnits != 0)
            InjectWheel(horizontalUnits, true);

        // Explorer and other shell views expect WHEEL_DELTA-sized messages. A sensitivity
        // value below one click is rounded up so a single physical notch never disappears.
        var explorerVerticalUnits = ToLegacyWheelDelta(explorerVertical);
        var explorerHorizontalUnits = ToLegacyWheelDelta(explorerHorizontal);
        if (explorerVerticalUnits != 0)
            InjectWheel(explorerVerticalUnits, false);
        if (explorerHorizontalUnits != 0)
            InjectWheel(explorerHorizontalUnits, true);
    }

    private static double Ease(double progress, FlowWheelOptions options)
    {
        if (!options.PulseAlgorithm) return progress;
        var scale = Math.Max(0.05d, options.SafePulseScale);
        var exponent = 1d + 2d / scale;
        var pulse = 1d - Math.Pow(1d - progress, exponent);
        // Normalize only the curve's shape. Preserve a final value of exactly 1 so the total
        // wheel amount does not change when users tune the pulse normalization setting.
        var normalized = Math.Max(0.0001d, options.SafePulseNormalize);
        var shaped = pulse * normalized + progress * (1d - normalized);
        return Math.Clamp(shaped, 0d, 1d);
    }

    private static int TakeWhole(ref double carry)
    {
        var whole = Math.Truncate(carry);
        carry -= whole;
        return (int)Math.Clamp(whole, int.MinValue, int.MaxValue);
    }

    private static int ToLegacyWheelDelta(double delta)
    {
        if (Math.Abs(delta) < 0.5d) return 0;
        var clicks = Math.Max(1, (int)Math.Round(Math.Abs(delta) / 120d, MidpointRounding.AwayFromZero));
        var units = Math.Min((long)int.MaxValue / 120L, clicks) * 120L;
        return (int)(Math.Sign(delta) * units);
    }

    private static void InjectWheel(int delta, bool horizontal)
    {
        var input = new NativeMethods.Input
        {
            Type = NativeMethods.INPUT_MOUSE,
            Mi = new NativeMethods.MouseInput
            {
                MouseData = unchecked((uint)delta),
                DwFlags = horizontal ? NativeMethods.MOUSEEVENTF_HWHEEL : NativeMethods.MOUSEEVENTF_WHEEL,
                DwExtraInfo = GlobalMouseHook.InjectionTag
            }
        };

        var sent = NativeMethods.SendInput(
            1,
            new[] { input },
            System.Runtime.InteropServices.Marshal.SizeOf<NativeMethods.Input>());
        // SendInput returns zero when blocked (for example by UIPI), but Windows does not
        // guarantee that GetLastError is populated for this case. Treat any short send as a
        // failure so AnimationThreadMain marks the engine faulted and the hook fails open.
        if (sent != 1)
        {
            var error = System.Runtime.InteropServices.Marshal.GetLastWin32Error();
            throw new System.ComponentModel.Win32Exception(
                error != 0 ? error : 5,
                $"FlowWheel could not inject a scroll pulse (sent {sent} of 1).");
        }
    }

    private sealed class ScrollSegment
    {
        internal ScrollSegment(double delta, bool horizontal, long startTimestamp, int durationMilliseconds, bool explorerCompatibility)
        {
            Delta = delta;
            Horizontal = horizontal;
            StartTimestamp = startTimestamp;
            DurationMilliseconds = durationMilliseconds;
            ExplorerCompatibility = explorerCompatibility;
        }

        internal double Delta { get; }
        internal bool Horizontal { get; }
        internal long StartTimestamp { get; }
        internal int DurationMilliseconds { get; }
        internal bool ExplorerCompatibility { get; }
        internal double Emitted { get; set; }
    }
}
