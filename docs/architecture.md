# FlowWheel clean-room architecture

FlowWheel is a native Windows tray utility that smooths mouse-wheel input for
the foreground desktop application. It is an independent implementation; the
legacy SmoothScroll executable and DLLs are not runtime dependencies and are
not inspected or reused by the build.

## Runtime shape

```text
FlowWheel.UI (WinForms, single tray process)
  ├─ TrayApplicationContext       menu, settings, enable/disable, exit
  ├─ SettingsController            load/validate/save JSON + change events
  ├─ LoginStartup                  HKCU Run registration (opt-in)
  └─ ScrollRuntime
       ├─ LowLevelMouseHook        WH_MOUSE_LL, short callback only
       ├─ ForegroundWindowTracker   process path for per-app policy
       └─ ScrollSmoother             timer/queue, easing, SendInput output

FlowWheel.Core (pure, testable)
  ├─ FlowWheelOptions               persisted settings model
  ├─ AppRule                        normalized executable rule
  ├─ ScrollPolicy                   global + per-app decision
  └─ ScrollMotion                   delta-to-frame smoothing math
```

The hook callback must never do file I/O, window enumeration, or sleeping. It
only filters injected events and enqueues a compact wheel sample. The worker
owns policy lookup and emits a bounded sequence of `SendInput` wheel events.
This keeps the hook under Windows' low-level-hook timeout and avoids a blocked
desktop when a settings file is malformed.

## Input/output contract

1. Install a `WH_MOUSE_LL` hook for the current interactive desktop.
2. For physical `WM_MOUSEWHEEL`/`WM_MOUSEHWHEEL` samples, read the signed
   high word of `mouseData`; do not consume injected samples (`LLMHF_INJECTED`).
3. Resolve the foreground process path and apply the global flag plus its
   optional executable rule. Unknown applications use the default profile.
4. For Shift + vertical wheel, route to horizontal output when
   `horizontalShiftKey` is enabled. Preserve native horizontal wheel input.
5. If smoothing is enabled, suppress the original event and emit fractional
   frames on a monotonic timer. The sum of emitted deltas equals the input
   delta (subject to integer wheel units). If smoothing is disabled, pass the
   event through unchanged.
6. Use `SendInput` with `MOUSEEVENTF_WHEEL` or `MOUSEEVENTF_HWHEEL` to deliver
   output to the current foreground window. Tag and ignore our injected output
   so it cannot recurse through the hook.

## Settings contract

Settings are stored at `%AppData%\\FlowWheel\\options.json` and written by an
atomic temp-file replace. The model intentionally mirrors the user-visible
controls discovered during read-only behavioral inspection, but uses the new
FlowWheel product name and schema:

```json
{
  "apps": {},
  "def": {
    "enabled": true,
    "stepSize": 120,
    "animationTime": 360,
    "pulseAlgorithm": true,
    "pulseNormalize": 1,
    "pulseScale": 3,
    "accelerationDefault": false,
    "accelerationDelta": 70,
    "accelerationMax": 7,
    "reverseDirection": false,
    "horizontalShiftKey": true,
    "horizontalSmoothing": true
  },
  "global": {
    "globalEnabled": true,
    "launchOnLogin": false,
    "showMenuBarIcon": true
  }
}
```

The loader must tolerate a missing file, unknown fields, and invalid numeric
values by falling back to validated defaults. App rules are keyed by normalized
full executable path and contain `enabled` plus an optional profile override.
No application content, keystrokes, or window text is collected.

## Tray and lifecycle

The process starts hidden with a single notification-area icon. Its menu should
include: `Enabled`, `Settings`, `Launch on login`, `Open settings folder`, and
`Exit FlowWheel`. A single-instance mutex prevents duplicate hooks. Shutdown
order is: stop smoother timer, unhook `WH_MOUSE_LL`, dispose notify icon, then
release the mutex. Settings changes are applied in memory immediately and
persisted on a debounced save.

Launch-on-login is implemented with the per-user
`HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run` value
`FlowWheel`, pointing at the installed executable with a quoted path. It never
requires elevation.

## Build and packaging

Target `net8.0-windows` with `UseWindowsForms=true`, `RuntimeIdentifier=win-x64`
and a self-contained single-file publish for the installable artifact. Keep
the installer separate from the app process; a small per-user installer may
copy the published directory, create the uninstall entry, and offer startup.
Unsigned builds should be clearly labeled as such because Windows SmartScreen
may warn on first launch.

## Safety and acceptance checks

- Hook install failure is visible in the tray tooltip and leaves the desktop
  untouched; it does not retry in a tight loop.
- The output queue is bounded; under load, it coalesces same-axis samples
  rather than growing without limit.
- `reverseDirection` is applied exactly once, before acceleration/easing.
- `stepSize` is clamped to 1..1200, `animationTime` to 0..2000 ms, and all
  settings are finite numbers.
- Disabling FlowWheel immediately restores pass-through behavior.
- Per-app disabled rules override the global default, while a global disabled
  state overrides every app rule.
- Shift-horizontal behavior and horizontal smoothing have independent toggles.
- A test harness can feed wheel samples to `ScrollMotion` without installing a
  global hook, making easing and conservation of total delta unit-testable.
