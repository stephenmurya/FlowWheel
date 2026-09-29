namespace FlowWheel.Core;

public readonly record struct ScreenPoint(int X, int Y);

public readonly record struct MouseWheelEvent(
    short Delta,
    bool Horizontal,
    ScreenPoint ScreenPoint,
    nint TargetWindow,
    uint Timestamp,
    bool ShiftDown);
