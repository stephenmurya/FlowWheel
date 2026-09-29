# Changelog

All notable changes to FlowWheel are documented here.

## [1.0.0] - 2026-09-29

Initial FlowWheel release for Windows.

### Added

- System-wide eased mouse-wheel scrolling through a low-level Windows mouse hook and `SendInput` output.
- Vertical and horizontal wheel smoothing, including Shift + wheel horizontal scrolling.
- Natural, Gentle, and Fast tuning presets.
- Sensitivity, duration, easing, acceleration, and direction controls.
- Tray enable/disable control and a settings window with **Reset defaults**.
- Foreground-app detection with persistent per-executable exclusions.
- Tray actions to disable or re-enable smoothing for the app under the pointer.
- Conservative automatic game bypass with explicit game exceptions.
- Optional launch-at-sign-in registration under the current user's `Run` key.
- Readable JSON preferences at `%APPDATA%\FlowWheel\options.json`.
- Per-user installer and portable self-contained `win-x64` executable.
- Single-instance protection and failure-open behavior: if hook setup or input injection fails, physical wheel input is passed through.

### Compatibility notes

- File Explorer receives complete wheel-click output for compatibility with legacy shell controls.
- Elevated applications may reject injected input because of Windows UIPI; run FlowWheel elevated only when that is appropriate for the user's environment.
- Automatic game detection is process/path based and is not an official Windows game-presence integration.

### Distribution

- Installer: `artifacts\installer\FlowWheel-Setup.exe`
- Portable executable: `artifacts\win-x64\FlowWheel.exe`
- Default install location: `%LOCALAPPDATA%\FlowWheel`

### Clean-room statement

FlowWheel 1.0.0 is an independent implementation. It does not ship or modify SmoothScroll binaries, licensing code, subscription checks, or proprietary assets.
