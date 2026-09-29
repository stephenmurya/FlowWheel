using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using Microsoft.Win32;

internal static class FlowWheelInstaller
{
    private const string Product = "FlowWheel";
    private const string UninstallKey = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\FlowWheel";
    private const string PayloadName = "FLOWWHEEL_APP";

    [STAThread]
    private static void Main(string[] args)
    {
        if (args.Length > 0 && string.Equals(args[0], "--help", StringComparison.OrdinalIgnoreCase))
        {
            Console.WriteLine("FlowWheel installer");
            Console.WriteLine("  FlowWheel-Setup.exe                 Install FlowWheel");
            Console.WriteLine("  FlowWheel-Setup.exe --install --target <folder> [--no-launch]");
            Console.WriteLine("  FlowWheel-Setup.exe --uninstall     Remove FlowWheel");
            return;
        }

        string target = DefaultInstallFolder();
        bool launch = true;
        bool install = false;
        bool uninstall = false;
        for (int index = 0; index < args.Length; index++)
        {
            if (string.Equals(args[index], "--install", StringComparison.OrdinalIgnoreCase)) install = true;
            else if (string.Equals(args[index], "--uninstall", StringComparison.OrdinalIgnoreCase)) uninstall = true;
            else if (string.Equals(args[index], "--no-launch", StringComparison.OrdinalIgnoreCase)) launch = false;
            else if (string.Equals(args[index], "--target", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length) target = args[++index];
        }

        try
        {
            if (uninstall)
            {
                Uninstall(target);
                return;
            }
            if (install)
            {
                Install(target, launch);
                return;
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new InstallerForm(target));
        }
        catch (Exception exception)
        {
            MessageBox.Show(exception.Message, Product + " setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
            Environment.ExitCode = 1;
        }
    }

    internal static string DefaultInstallFolder()
    {
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), Product);
    }

    internal static void Install(string target, bool launch)
    {
        target = Path.GetFullPath(target);
        Directory.CreateDirectory(target);
        string appPath = Path.Combine(target, Product + ".exe");
        using (Stream input = Assembly.GetExecutingAssembly().GetManifestResourceStream(PayloadName))
        {
            if (input == null) throw new InvalidOperationException("The FlowWheel application payload is missing.");
            using (FileStream output = File.Create(appPath)) input.CopyTo(output);
        }

        string self = Assembly.GetExecutingAssembly().Location;
        string setupPath = Path.Combine(target, Product + "-Setup.exe");
        if (!string.Equals(Path.GetFullPath(self), Path.GetFullPath(setupPath), StringComparison.OrdinalIgnoreCase))
            File.Copy(self, setupPath, true);

        CreateShortcut(appPath);
        using (RegistryKey key = Registry.CurrentUser.CreateSubKey(UninstallKey))
        {
            if (key != null)
            {
                key.SetValue("DisplayName", Product);
                key.SetValue("DisplayVersion", "2.0.0");
                key.SetValue("Publisher", Product);
                key.SetValue("InstallLocation", target);
                key.SetValue("UninstallString", "\"" + setupPath + "\" --uninstall");
                key.SetValue("NoModify", 1, RegistryValueKind.DWord);
                key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
            }
        }

        if (launch) Process.Start(appPath);
    }

    private static void CreateShortcut(string appPath)
    {
        string menu = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Microsoft", "Windows", "Start Menu", "Programs");
        Directory.CreateDirectory(menu);
        string shortcut = Path.Combine(menu, Product + ".lnk");
        Type shellType = Type.GetTypeFromProgID("WScript.Shell");
        if (shellType == null) return;
        dynamic shell = Activator.CreateInstance(shellType);
        dynamic link = shell.CreateShortcut(shortcut);
        link.TargetPath = appPath;
        link.WorkingDirectory = Path.GetDirectoryName(appPath);
        link.Description = "Smooth scrolling for Windows";
        link.Save();
    }

    internal static void Uninstall(string target)
    {
        target = Path.GetFullPath(target);
        string shortcut = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Microsoft", "Windows", "Start Menu", "Programs", Product + ".lnk");
        if (File.Exists(shortcut)) File.Delete(shortcut);
        Registry.CurrentUser.DeleteSubKeyTree(UninstallKey, false);

        string current = Path.GetFullPath(Assembly.GetExecutingAssembly().Location);
        string script = Path.Combine(Path.GetTempPath(), Product + "-remove-" + Guid.NewGuid().ToString("N") + ".cmd");
        File.WriteAllText(script, "@echo off\r\nping 127.0.0.1 -n 2 >nul\r\nrmdir /s /q \"" + target + "\"\r\ndel \"%~f0\"\r\n");
        Process.Start(new ProcessStartInfo("cmd.exe", "/c \"" + script + "\"")
        {
            CreateNoWindow = true,
            UseShellExecute = false,
            WindowStyle = ProcessWindowStyle.Hidden
        });
    }

    private sealed class InstallerForm : Form
    {
        private readonly string _target;
        private readonly CheckBox _launch;

        internal InstallerForm(string target)
        {
            _target = target;
            Text = "FlowWheel setup";
            ClientSize = new Size(480, 270);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(20, 21, 27);
            ForeColor = Color.White;

            Controls.Add(new Label { Text = "FlowWheel", Font = new Font("Segoe UI Semibold", 22), AutoSize = true, Location = new Point(32, 26), ForeColor = Color.FromArgb(90, 220, 190) });
            Controls.Add(new Label { Text = "Smooth scrolling for Windows", Font = new Font("Segoe UI", 11), AutoSize = true, Location = new Point(35, 72), ForeColor = Color.FromArgb(190, 194, 204) });
            Controls.Add(new Label { Text = "FlowWheel runs quietly in the notification area and applies\nsmooth scrolling across your Windows desktop.", AutoSize = true, Location = new Point(35, 112), ForeColor = Color.FromArgb(190, 194, 204) });
            _launch = new CheckBox { Text = "Launch FlowWheel after setup", Checked = true, AutoSize = true, Location = new Point(35, 173), ForeColor = Color.FromArgb(225, 227, 232) };
            Controls.Add(_launch);
            Button install = new Button { Text = "Install", DialogResult = DialogResult.OK, Width = 104, Height = 32, Location = new Point(335, 215), BackColor = Color.FromArgb(90, 220, 190), FlatStyle = FlatStyle.Flat };
            install.FlatAppearance.BorderSize = 0;
            install.Click += delegate { FlowWheelInstaller.Install(_target, _launch.Checked); Close(); };
            Controls.Add(install);
            AcceptButton = install;
        }
    }
}
