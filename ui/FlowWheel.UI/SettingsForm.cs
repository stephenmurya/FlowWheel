namespace FlowWheel;

internal sealed class SettingsForm : Form
{
    private static readonly Color Background = Color.FromArgb(18, 19, 24);
    private static readonly Color Panel = Color.FromArgb(29, 30, 38);
    private static readonly Color TextColor = Color.FromArgb(242, 244, 247);
    private static readonly Color Muted = Color.FromArgb(157, 163, 176);
    private static readonly Color Accent = Color.FromArgb(90, 220, 190);
    private readonly AppSettings _settings;
    private readonly Action _onSave;
    private CheckBox _enabled = null!;
    private TrackBar _sensitivity = null!;
    private TrackBar _duration = null!;
    private TrackBar _acceleration = null!;
    private CheckBox _invert = null!;
    private CheckBox _horizontal = null!;
    private CheckBox _shiftHorizontal = null!;
    private CheckBox _startup = null!;
    private CheckBox _notifications = null!;
    private CheckBox _games = null!;
    private ComboBox _easing = null!;
    private ComboBox _preset = null!;
    private TextBox _excludedApps = null!;
    private TextBox _gameExceptions = null!;
    private Label _sensitivityValue = null!;
    private Label _durationValue = null!;
    private Label _accelerationValue = null!;

    public SettingsForm(AppSettings settings, Action onSave)
    {
        _settings = settings;
        _onSave = onSave;
        Text = "FlowWheel Settings";
        BackColor = Background;
        ForeColor = TextColor;
        Font = new Font("Segoe UI", 9.5f);
        ClientSize = new Size(660, 660);
        MinimumSize = new Size(620, 580);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        ShowInTaskbar = true;

        var root = new TableLayoutPanel { Dock = DockStyle.Fill, BackColor = Background, Padding = new Padding(30, 24, 30, 22), RowCount = 4, ColumnCount = 1 };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 68));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
        Controls.Add(root);

        var header = new Panel { Dock = DockStyle.Fill };
        header.Controls.Add(new Label { Text = "FlowWheel", Font = new Font("Segoe UI Semibold", 22), ForeColor = TextColor, AutoSize = true, Location = new Point(0, 0) });
        header.Controls.Add(new Label { Text = "Make every scroll feel natural.", Font = new Font("Segoe UI", 10), ForeColor = Muted, AutoSize = true, Location = new Point(2, 39) });
        root.Controls.Add(header, 0, 0);

        var status = new Panel { Dock = DockStyle.Fill, BackColor = Panel, Padding = new Padding(18, 12, 18, 10) };
        _enabled = MakeCheck("Smooth scrolling", _settings.Enabled);
        _enabled.Font = new Font("Segoe UI Semibold", 11);
        _enabled.ForeColor = TextColor;
        _enabled.CheckedChanged += (_, _) => { _settings.Enabled = _enabled.Checked; Save(); };
        status.Controls.Add(_enabled);
        status.Controls.Add(new Label { Text = "Active for mouse and touchpad input across Windows", ForeColor = Muted, AutoSize = true, Location = new Point(25, 49) });
        root.Controls.Add(status, 0, 1);

        var body = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Padding = new Padding(0, 18, 0, 0) };
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58)); body.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42));
        body.Controls.Add(BuildTuningPanel(), 0, 0); body.Controls.Add(BuildOptionsPanel(), 1, 0);
        root.Controls.Add(body, 0, 2);

        var footer = new Panel { Dock = DockStyle.Fill };
        _startup = MakeCheck("Launch FlowWheel when Windows starts", _settings.StartWithWindows);
        _startup.ForeColor = Muted;
        _startup.Dock = DockStyle.Left;
        _startup.CheckedChanged += (_, _) => { _settings.StartWithWindows = _startup.Checked; Save(); };
        footer.Controls.Add(_startup);
        var reset = new Button { Text = "Reset defaults", AutoSize = true, FlatStyle = FlatStyle.Flat, BackColor = Panel, ForeColor = TextColor, Dock = DockStyle.Right, Padding = new Padding(10, 4, 10, 4), Margin = new Padding(0, 0, 8, 0) };
        reset.FlatAppearance.BorderColor = Color.FromArgb(65, 68, 78);
        reset.Click += (_, _) => ResetDefaults();
        footer.Controls.Add(reset);
        var close = new Button { Text = "Done", AutoSize = true, FlatStyle = FlatStyle.Flat, BackColor = Accent, ForeColor = Color.FromArgb(11, 35, 31), Font = new Font("Segoe UI Semibold", 9.5f), Dock = DockStyle.Right, Padding = new Padding(18, 4, 18, 4) };
        close.FlatAppearance.BorderSize = 0; close.Click += (_, _) => Close(); footer.Controls.Add(close);
        root.Controls.Add(footer, 0, 3);
    }

    private Control BuildTuningPanel()
    {
        var panel = new Panel { Dock = DockStyle.Fill, Padding = new Padding(0, 0, 16, 0) };
        panel.Controls.Add(new Label { Text = "TUNING", ForeColor = Accent, Font = new Font("Segoe UI Semibold", 8.5f), AutoSize = true, Location = new Point(0, 0) });
        panel.Controls.Add(new Label { Text = "Profile", ForeColor = TextColor, AutoSize = true, Location = new Point(126, 0) });
        _preset = new ComboBox { Location = new Point(126, 18), Width = 142, DropDownStyle = ComboBoxStyle.DropDownList, BackColor = Color.FromArgb(40, 42, 51), ForeColor = TextColor, FlatStyle = FlatStyle.Flat };
        _preset.Items.AddRange(new object[] { AppSettings.NaturalPreset, AppSettings.GentlePreset, AppSettings.FastPreset });
        _preset.SelectedItem = _settings.Preset; if (_preset.SelectedIndex < 0) _preset.SelectedIndex = 0;
        _preset.SelectedIndexChanged += (_, _) => ApplyPreset(_preset.SelectedItem?.ToString() ?? AppSettings.NaturalPreset);
        panel.Controls.Add(_preset);
        _sensitivityValue = new Label { ForeColor = Muted, AutoSize = true, Location = new Point(220, 40) };
        _durationValue = new Label { ForeColor = Muted, AutoSize = true, Location = new Point(220, 122) };
        _accelerationValue = new Label { ForeColor = Muted, AutoSize = true, Location = new Point(220, 204) };
        _sensitivity = MakeSlider(_settings.Sensitivity, 5, 100); _duration = MakeSlider(_settings.Duration, 80, 600); _acceleration = MakeSlider(_settings.Acceleration, 0, 100);
        AddSlider(panel, "Sensitivity", "How far each wheel tick travels", 66, _sensitivity, _sensitivityValue, v => $"{v}%", v => { _settings.Sensitivity = v; _settings.StepSize = v; });
        AddSlider(panel, "Duration", "Time to settle after each tick", 148, _duration, _durationValue, v => $"{v} ms", v => _settings.Duration = v);
        AddSlider(panel, "Acceleration", "Build speed during continuous scrolling", 230, _acceleration, _accelerationValue, v => $"{v}%", v => { _settings.Acceleration = v; _settings.AccelerationDelta = Math.Max(1, 100 - v); });
        return panel;
    }

    private Control BuildOptionsPanel()
    {
        var panel = new Panel { Dock = DockStyle.Fill, Padding = new Padding(14, 0, 0, 0) };
        panel.Controls.Add(new Label { Text = "INPUTS", ForeColor = Accent, Font = new Font("Segoe UI Semibold", 8.5f), AutoSize = true, Location = new Point(14, 0) });
        _horizontal = MakeCheck("Horizontal scrolling", _settings.SmoothHorizontal); _horizontal.Location = new Point(14, 34); _horizontal.CheckedChanged += (_, _) => { _settings.SmoothHorizontal = _horizontal.Checked; Save(); };
        _shiftHorizontal = MakeCheck("Shift + wheel horizontally", _settings.ShiftHorizontal); _shiftHorizontal.Location = new Point(14, 74); _shiftHorizontal.CheckedChanged += (_, _) => { _settings.ShiftHorizontal = _shiftHorizontal.Checked; Save(); };
        _invert = MakeCheck("Invert direction", _settings.InvertDirection); _invert.Location = new Point(14, 114); _invert.CheckedChanged += (_, _) => { _settings.InvertDirection = _invert.Checked; Save(); };
        _notifications = MakeCheck("Show tray notifications", _settings.ShowNotifications); _notifications.Location = new Point(14, 154); _notifications.CheckedChanged += (_, _) => { _settings.ShowNotifications = _notifications.Checked; Save(); };
        panel.Controls.AddRange(new Control[] { _horizontal, _shiftHorizontal, _invert, _notifications });

        _games = MakeCheck("Pause in detected games", _settings.AutoDisableGames); _games.Location = new Point(14, 184);
        _games.CheckedChanged += (_, _) => { _settings.AutoDisableGames = _games.Checked; Save(); };
        panel.Controls.Add(_games);

        panel.Controls.Add(new Label { Text = "Easing", ForeColor = TextColor, AutoSize = true, Location = new Point(14, 214) });
        _easing = new ComboBox { Location = new Point(14, 238), Width = 170, DropDownStyle = ComboBoxStyle.DropDownList, BackColor = Color.FromArgb(40, 42, 51), ForeColor = TextColor, FlatStyle = FlatStyle.Flat };
        _easing.Items.AddRange(new object[] { "Cubic", "Smooth", "Linear", "Spring" });
        _easing.SelectedItem = _settings.Easing; if (_easing.SelectedIndex < 0) _easing.SelectedIndex = 0;
        _easing.SelectedIndexChanged += (_, _) => { _settings.Easing = _easing.SelectedItem?.ToString() ?? "Cubic"; Save(); };
        panel.Controls.Add(_easing);

        panel.Controls.Add(new Label { Text = "Excluded apps (process names, comma separated)", ForeColor = TextColor, AutoSize = true, Location = new Point(14, 278) });
        _excludedApps = new TextBox { Location = new Point(14, 302), Width = 245, BackColor = Color.FromArgb(40, 42, 51), ForeColor = TextColor, BorderStyle = BorderStyle.FixedSingle, Text = _settings.ExcludedApps };
        _excludedApps.Leave += (_, _) => { _settings.ExcludedApps = _excludedApps.Text.Trim(); Save(); };
        panel.Controls.Add(_excludedApps);
        panel.Controls.Add(new Label { Text = "Game bypass exceptions (names or paths)", ForeColor = TextColor, AutoSize = true, Location = new Point(14, 334) });
        _gameExceptions = new TextBox { Location = new Point(14, 358), Width = 245, BackColor = Color.FromArgb(40, 42, 51), ForeColor = TextColor, BorderStyle = BorderStyle.FixedSingle, Text = _settings.GameBypassExceptions };
        _gameExceptions.Leave += (_, _) => { _settings.GameBypassExceptions = _gameExceptions.Text.Trim(); Save(); };
        panel.Controls.Add(_gameExceptions);
        return panel;
    }

    private void AddSlider(Panel panel, string title, string subtitle, int y, TrackBar slider, Label value, Func<int, string> format, Action<int> assign)
    {
        var label = new Label { Text = title, ForeColor = TextColor, AutoSize = true, Location = new Point(0, y) };
        var sub = new Label { Text = subtitle, ForeColor = Muted, AutoSize = true, Location = new Point(0, y + 22) };
        value.Location = new Point(220, y); value.Text = format(slider.Value); value.TextAlign = ContentAlignment.TopRight; value.Width = 55;
        slider.Location = new Point(0, y + 42); slider.Width = 280;
        slider.ValueChanged += (_, _) => { value.Text = format(slider.Value); assign(slider.Value); Save(); };
        panel.Controls.AddRange(new Control[] { label, value, sub, slider });
    }

    private static TrackBar MakeSlider(int value, int min, int max) => new() { Minimum = min, Maximum = max, Value = Math.Clamp(value, min, max), TickStyle = TickStyle.None, BackColor = Panel };
    private static CheckBox MakeCheck(string text, bool value) => new() { Text = text, Checked = value, AutoSize = true, ForeColor = Muted, FlatStyle = FlatStyle.Standard };
    private void ApplyPreset(string preset)
    {
        _settings.ApplyPreset(preset);
        _sensitivity.Value = Math.Clamp(_settings.Sensitivity, _sensitivity.Minimum, _sensitivity.Maximum);
        _duration.Value = Math.Clamp(_settings.Duration, _duration.Minimum, _duration.Maximum);
        _acceleration.Value = Math.Clamp(_settings.Acceleration, _acceleration.Minimum, _acceleration.Maximum);
        _easing.SelectedItem = _settings.Easing;
        Save();
    }

    private void ResetDefaults()
    {
        if (MessageBox.Show("Reset all FlowWheel settings to the Natural defaults?", "Reset defaults", MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK)
            return;
        _settings.ResetToDefaults();
        _preset.SelectedItem = _settings.Preset;
        _sensitivity.Value = _settings.Sensitivity;
        _duration.Value = _settings.Duration;
        _acceleration.Value = _settings.Acceleration;
        _easing.SelectedItem = _settings.Easing;
        _games.Checked = _settings.AutoDisableGames;
        _horizontal.Checked = _settings.SmoothHorizontal;
        _shiftHorizontal.Checked = _settings.ShiftHorizontal;
        _invert.Checked = _settings.InvertDirection;
        _notifications.Checked = _settings.ShowNotifications;
        _startup.Checked = _settings.StartWithWindows;
        _excludedApps.Text = _settings.ExcludedApps;
        _gameExceptions.Text = _settings.GameBypassExceptions;
        Save();
    }
    private void Save() { SettingsStore.Save(_settings); _onSave(); }
}
