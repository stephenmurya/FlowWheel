using System.Drawing.Drawing2D;
using System.Security;
using Microsoft.Win32;
using FlowWheel.Core;

namespace FlowWheel;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        using var instanceMutex = new Mutex(true, @"Local\FlowWheel", out var createdNew);
        if (!createdNew) return;
        try
        {
            Application.Run(new FlowWheelContext());
        }
        finally
        {
            instanceMutex.ReleaseMutex();
        }
    }
}

internal sealed class FlowWheelContext : ApplicationContext
{
    private readonly NotifyIcon _tray;
    private readonly AppSettings _settings;
    private readonly SmoothScrollEngine _engine;
    private readonly SynchronizationContext _uiContext;
    private readonly int _ownProcessId = Environment.ProcessId;
    private readonly System.Windows.Forms.Timer _activeWindowTimer;
    private int _faultShown;
    private bool _shuttingDown;
    private SettingsForm? _settingsForm;
    private AppTarget? _lastAppTarget;
    private AppTarget? _menuTarget;
    private ToolStripMenuItem? _appRuleMenuItem;

    public FlowWheelContext()
    {
        _uiContext = SynchronizationContext.Current ?? new WindowsFormsSynchronizationContext();
        _settings = SettingsStore.Load();
        _engine = new SmoothScrollEngine(ToCoreOptions(_settings));
        _engine.Faulted += OnEngineFault;
        _tray = new NotifyIcon
        {
            Icon = CreateTrayIcon(),
            Text = "FlowWheel — smooth scrolling",
            Visible = true,
            ContextMenuStrip = BuildMenu()
        };
        _tray.DoubleClick += (_, _) => ShowSettings();
        _activeWindowTimer = new System.Windows.Forms.Timer { Interval = 250 };
        _activeWindowTimer.Tick += (_, _) => RememberActiveTarget();
        _activeWindowTimer.Start();
        RememberActiveTarget();
        SetStartup(_settings.StartWithWindows);
        try { _engine.Start(); }
        catch (Exception exception) { OnEngineFault(exception); }
    }

    private ContextMenuStrip BuildMenu()
    {
        var menu = new ContextMenuStrip { ShowImageMargin = false };
        var enabled = new ToolStripMenuItem("Smooth scrolling") { Checked = _settings.Enabled, CheckOnClick = true };
        enabled.Click += (_, _) => { _settings.Enabled = enabled.Checked; SaveAndNotify(); };
        menu.Items.Add(enabled);
        menu.Items.Add(new ToolStripSeparator());
        _appRuleMenuItem = new ToolStripMenuItem("App scrolling rule unavailable") { Enabled = false };
        _appRuleMenuItem.Click += (_, _) => ToggleAppRule();
        menu.Items.Add(_appRuleMenuItem);
        menu.Opening += (_, _) => UpdateAppRuleMenu();
        menu.Items.Add(new ToolStripSeparator());
        var settings = new ToolStripMenuItem("Settings…");
        settings.Click += (_, _) => ShowSettings();
        menu.Items.Add(settings);
        var about = new ToolStripMenuItem("About FlowWheel");
        about.Click += (_, _) => MessageBox.Show("FlowWheel\nSmooth scrolling for Windows", "About FlowWheel", MessageBoxButtons.OK, MessageBoxIcon.Information);
        menu.Items.Add(about);
        menu.Items.Add(new ToolStripSeparator());
        var exit = new ToolStripMenuItem("Exit");
        exit.Click += (_, _) => ExitThread();
        menu.Items.Add(exit);
        return menu;
    }

    private void RememberActiveTarget()
    {
        if (_shuttingDown) return;
        var target = AppTargetResolver.GetForegroundTarget(_ownProcessId);
        if (target is not null)
            _lastAppTarget = target;
    }

    private void UpdateAppRuleMenu()
    {
        if (_appRuleMenuItem is null) return;

        // Opening the tray menu can temporarily make the shell's notification-area window
        // foreground. Prefer the window under the pointer when it is a real app, otherwise
        // use the last foreground app captured by the timer.
        _menuTarget = AppTargetResolver.GetTargetAt(Cursor.Position, _ownProcessId) ?? _lastAppTarget;
        if (_menuTarget is null)
        {
            _appRuleMenuItem.Enabled = false;
            _appRuleMenuItem.Text = "App scrolling rule unavailable";
            return;
        }

        _appRuleMenuItem.Enabled = true;
        _appRuleMenuItem.Text = $"{(IsExcluded(_menuTarget) ? "Enable" : "Disable")} smooth scrolling in {_menuTarget.DisplayName}";
    }

    private void ToggleAppRule()
    {
        var target = _menuTarget;
        if (target is null) return;

        var explicitExcluded = IsExplicitlyExcluded(target);
        var autoGameExcluded = IsAutoGameExcluded(target);
        if (autoGameExcluded && !explicitExcluded)
        {
            // An automatic game bypass is opt-out per executable: adding an exception keeps
            // smoothing on for this game while preserving the global safety default.
            var rules = ParseLegacyRules(_settings.GameBypassExceptions).ToList();
            var key = target.HasExecutablePath ? NormalizePath(target.ExecutablePath) : NormalizeProcessName(target.ProcessName);
            if (!rules.Any(rule => string.Equals(NormalizePath(rule), key, StringComparison.OrdinalIgnoreCase) ||
                                   string.Equals(NormalizeProcessName(rule), key, StringComparison.OrdinalIgnoreCase)))
                rules.Add(key);
            _settings.GameBypassExceptions = string.Join(", ", rules);
        }
        else if (explicitExcluded)
        {
            if (target.HasExecutablePath)
            {
                var normalized = NormalizePath(target.ExecutablePath);
                _settings.ExcludedAppPaths.RemoveAll(path =>
                    string.Equals(NormalizePath(path), normalized, StringComparison.OrdinalIgnoreCase));
            }

            var processName = NormalizeProcessName(target.ProcessName);
            _settings.ExcludedApps = string.Join(", ", ParseLegacyRules(_settings.ExcludedApps)
                .Where(rule => !string.Equals(NormalizeProcessName(rule), processName, StringComparison.OrdinalIgnoreCase) &&
                               !string.Equals(NormalizePath(rule), NormalizePath(target.ExecutablePath), StringComparison.OrdinalIgnoreCase)));
            if (autoGameExcluded)
                AddGameBypassException(target);
        }
        else if (target.HasExecutablePath)
        {
            var normalized = NormalizePath(target.ExecutablePath);
            if (!_settings.ExcludedAppPaths.Any(path => string.Equals(NormalizePath(path), normalized, StringComparison.OrdinalIgnoreCase)))
                _settings.ExcludedAppPaths.Add(normalized);
        }
        else
        {
            var processName = NormalizeProcessName(target.ProcessName);
            var rules = ParseLegacyRules(_settings.ExcludedApps).ToList();
            if (!rules.Any(rule => string.Equals(NormalizeProcessName(rule), processName, StringComparison.OrdinalIgnoreCase)))
                rules.Add(processName);
            _settings.ExcludedApps = string.Join(", ", rules);
        }

        SaveAndNotify();
        UpdateAppRuleMenu();
    }

    private bool IsExcluded(AppTarget target)
    {
        return IsExplicitlyExcluded(target) || IsAutoGameExcluded(target);
    }

    private bool IsExplicitlyExcluded(AppTarget target)
    {
        var processName = NormalizeProcessName(target.ProcessName);
        if (ParseLegacyRules(_settings.ExcludedApps).Any(rule =>
                string.Equals(NormalizeProcessName(rule), processName, StringComparison.OrdinalIgnoreCase) ||
                (target.HasExecutablePath && string.Equals(NormalizePath(rule), NormalizePath(target.ExecutablePath), StringComparison.OrdinalIgnoreCase))))
            return true;

        return target.HasExecutablePath && _settings.ExcludedAppPaths.Any(path =>
            string.Equals(NormalizePath(path), NormalizePath(target.ExecutablePath), StringComparison.OrdinalIgnoreCase));
    }

    private bool IsAutoGameExcluded(AppTarget target)
    {
        if (!_settings.AutoDisableGames || IsGameException(target)) return false;
        return GameBypassClassifier.IsGameProcess(target.ProcessName, target.ExecutablePath);
    }

    private void AddGameBypassException(AppTarget target)
    {
        var rules = ParseLegacyRules(_settings.GameBypassExceptions).ToList();
        var key = target.HasExecutablePath ? NormalizePath(target.ExecutablePath) : NormalizeProcessName(target.ProcessName);
        if (!rules.Any(rule => string.Equals(NormalizePath(rule), key, StringComparison.OrdinalIgnoreCase) ||
                               string.Equals(NormalizeProcessName(rule), key, StringComparison.OrdinalIgnoreCase)))
            rules.Add(key);
        _settings.GameBypassExceptions = string.Join(", ", rules);
    }

    private bool IsGameException(AppTarget target)
    {
        var processName = NormalizeProcessName(target.ProcessName);
        var processPath = NormalizePath(target.ExecutablePath);
        return ParseLegacyRules(_settings.GameBypassExceptions).Any(rule =>
            string.Equals(NormalizeProcessName(rule), processName, StringComparison.OrdinalIgnoreCase) ||
            (target.HasExecutablePath && string.Equals(NormalizePath(rule), processPath, StringComparison.OrdinalIgnoreCase)));
    }

    private static IEnumerable<string> ParseLegacyRules(string? value) =>
        (value ?? string.Empty).Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

    private static string NormalizeProcessName(string value)
    {
        var name = Path.GetFileName(value.Trim());
        return name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? name : name + ".exe";
    }

    private static string NormalizePath(string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) return string.Empty;
        try { return Path.GetFullPath(value.Trim()); }
        catch { return value.Trim(); }
    }

    private void ShowSettings()
    {
        if (_settingsForm is { IsDisposed: false }) { _settingsForm.Activate(); return; }
        _settingsForm = new SettingsForm(_settings, SaveAndNotify);
        _settingsForm.FormClosed += (_, _) => _settingsForm = null;
        _settingsForm.Show();
    }

    private void SaveAndNotify()
    {
        SettingsStore.Save(_settings);
        _engine.UpdateOptions(ToCoreOptions(_settings));
        SetStartup(_settings.StartWithWindows);
        if (_settings.ShowNotifications)
            _tray.ShowBalloonTip(1600, "FlowWheel", _settings.Enabled ? "Smooth scrolling is on" : "Smooth scrolling is paused", ToolTipIcon.Info);
    }

    private void OnEngineFault(Exception exception)
    {
        // Hook and animator faults originate on worker threads. Marshal to the WinForms context
        // before touching NotifyIcon, and only show the first fault to avoid balloon storms.
        try { _uiContext.Post(_ => ShowEngineFault(exception), null); } catch (ObjectDisposedException) { }
    }

    private void ShowEngineFault(Exception exception)
    {
        if (_shuttingDown || Interlocked.Exchange(ref _faultShown, 1) != 0) return;
        if (_settings.ShowNotifications)
            _tray.ShowBalloonTip(2400, "FlowWheel", "Smooth scrolling could not start: " + exception.Message, ToolTipIcon.Warning);
    }

    protected override void ExitThreadCore()
    {
        _shuttingDown = true;
        _activeWindowTimer.Stop();
        _activeWindowTimer.Dispose();
        _engine.Dispose();
        _tray.Visible = false;
        _tray.Dispose();
        base.ExitThreadCore();
    }

    private static FlowWheelOptions ToCoreOptions(AppSettings settings)
    {
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var gameExceptions = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var entry in settings.ExcludedApps.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            if (entry.Contains('\\') || entry.Contains('/') || Path.IsPathRooted(entry))
                paths.Add(NormalizePath(entry));
            else
                names.Add(entry.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? entry : entry + ".exe");
        }
        foreach (var entry in settings.ExcludedAppPaths ?? new List<string>())
        {
            if (!string.IsNullOrWhiteSpace(entry)) paths.Add(NormalizePath(entry));
        }
        foreach (var entry in settings.GameBypassExceptions.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            if (entry.Contains('\\') || entry.Contains('/') || Path.IsPathRooted(entry))
                gameExceptions.Add(NormalizePath(entry));
            else
                gameExceptions.Add(entry.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? entry : entry + ".exe");
        }

        return new FlowWheelOptions
        {
            Enabled = settings.Enabled,
            GlobalEnabled = settings.Enabled,
            StepSize = 120d * settings.Sensitivity / 50d,
            AnimationTime = settings.Duration,
            PulseAlgorithm = !string.Equals(settings.Easing, "Linear", StringComparison.OrdinalIgnoreCase),
            PulseScale = string.Equals(settings.Easing, "Smooth", StringComparison.OrdinalIgnoreCase) ? 1.5d : 3d,
            AccelerationDefault = settings.Acceleration > 0,
            AccelerationDelta = Math.Max(1d, settings.AccelerationDelta),
            AccelerationMax = 1d + settings.Acceleration / 15d,
            ReverseDirection = settings.InvertDirection,
            HorizontalShiftKey = settings.ShiftHorizontal,
            HorizontalSmoothing = settings.SmoothHorizontal,
            ExcludedProcessNames = names,
            ExcludedProcessPaths = paths,
            AutoDisableGames = settings.AutoDisableGames,
            GameBypassExceptions = gameExceptions
        };
    }

    private static void SetStartup(bool enabled)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Run");
            if (key is null) return;
            if (enabled)
            {
                var path = Environment.ProcessPath ?? Application.ExecutablePath;
                key.SetValue("FlowWheel", $"\"{path}\"");
            }
            else key.DeleteValue("FlowWheel", false);
        }
        catch (UnauthorizedAccessException) { }
        catch (SecurityException) { }
    }

    private static Icon CreateTrayIcon()
    {
        using var bitmap = new Bitmap(32, 32);
        using (var graphics = Graphics.FromImage(bitmap))
        {
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            graphics.Clear(Color.Transparent);
            using var brush = new SolidBrush(Color.FromArgb(38, 38, 48));
            graphics.FillEllipse(brush, 1, 1, 30, 30);
            using var accent = new Pen(Color.FromArgb(90, 220, 190), 3);
            graphics.DrawArc(accent, 7, 7, 18, 18, 35, 250);
            graphics.DrawLine(accent, 18, 5, 25, 9);
            graphics.DrawLine(accent, 25, 9, 23, 17);
        }
        return Icon.FromHandle(bitmap.GetHicon());
    }
}
