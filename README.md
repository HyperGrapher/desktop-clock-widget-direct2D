# Desktop Clock Widget

A native Windows desktop date/clock widget rendered with Direct2D and animated by
DirectComposition. It uses a borderless Win32 window, premultiplied per-pixel alpha,
click-through input, a circular glass face, and the documented Windows App SDK Desktop
Acrylic backdrop.

The hour, minute, and second hands are separate DirectComposition visuals. Their rotation
runs inside the Windows compositor, so the application does not repaint the complete clock
at 60 FPS. The date follows the current Windows locale and is refreshed at local midnight.

## Features

- Classic analog face with hour, minute, and continuously sweeping second hands
- Direct2D and DirectWrite rendering over BGRA Direct3D 11/DXGI surfaces
- Native DirectComposition visual tree and compositor-driven hand animations
- Premultiplied per-pixel alpha with a circular transparent Win32 window
- Windows App SDK thin Desktop Acrylic on supported Windows 10 and Windows 11 systems
- Translucent tinted fallback when acrylic is unavailable or disabled by system policy
- Per-monitor-v2 DPI scaling and multi-monitor position persistence
- Notification-area controls for click-through locking, dragging, always-on-top, reset,
  and exit
- Recovery after DPI changes, display changes, clock changes, and sleep/resume

## Controls

The widget starts locked and click-through. Use its notification-area icon to control it:

- **Click-through / locked**: toggle this off to make the widget draggable. Drag anywhere
  on the clock face, then lock it again.
- **Always on top**: toggle whether the widget stays above ordinary application windows.
- **Reset position**: move the widget to the top-right of the primary monitor.
- **Exit**: save the current state and close the application.

Position, lock state, and always-on-top state are stored in:

```text
%LOCALAPPDATA%\DesktopClockWidget\settings.ini
```

## Requirements

- Windows 10 version 1809 (build 17763) or newer; Windows 10 22H2 or Windows 11 is
  recommended
- Visual Studio 2022 with **Desktop development with C++**
- MSVC v143 toolset
- Windows 10/11 SDK 10.0.19041.0 or newer
- CMake 3.24 or newer
- An x64 Windows App SDK 1.8 runtime on the computer that runs the widget

The build restores these pinned NuGet packages automatically:

- `Microsoft.WindowsAppSDK.Foundation` 1.8.260222000
- `Microsoft.WindowsAppSDK.Runtime` 1.8.260317003
- `Microsoft.Windows.CppWinRT` 2.0.250303.1

The Windows App SDK runtime can be installed from Microsoft's
[Windows App SDK downloads](https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads).
The generated output also contains `Microsoft.WindowsAppRuntime.Bootstrap.dll`; keep it next
to the executable when copying the application.

Direct2D, DirectComposition, DirectWrite, Direct3D, and DXGI are Windows SDK components.
There are no vcpkg dependencies, so a vcpkg manifest is not required.

## Build

Run the following commands from a normal PowerShell window or a Visual Studio Developer
PowerShell. CMake locates the installed Visual Studio toolchain and NuGet restores the
Windows App SDK packages during the first build.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

The application is written to:

```text
build\Release\DesktopClockWidget.exe
```

The first build can take longer while NuGet downloads and extracts the Windows App SDK.
If restore is interrupted by a network error, run the build command again; completed
packages remain in the local NuGet cache.

## Test

Configure with testing enabled (the default), build, and run CTest:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The native tests cover fractional hand angles, noon/midnight behavior, continuous
minute/hour movement, and midnight scheduling.

## Run

```powershell
.\build\Release\DesktopClockWidget.exe
```

If startup reports that the Windows App SDK framework cannot be found, install the x64
Windows App SDK 1.8 runtime and launch the executable again.

## Architecture

- `WidgetWindow` owns the Win32 HWND, DPI/display lifecycle, tray icon, hit testing, and
  persisted settings.
- `CompositionRenderer` creates the Direct3D/DXGI/Direct2D device stack, transparent swap
  chains, DirectComposition visuals, and repeating hand animations.
- `AcrylicHost` creates the bottom Windows Composition target used by the documented
  Desktop Acrylic controller. The native DirectComposition clock target is placed above it.
- `ClockMath` contains the independently tested wall-clock angle and midnight calculations.

The app intentionally uses `WS_EX_NOREDIRECTIONBITMAP` rather than the legacy
`UpdateLayeredWindow` path. DirectComposition supplies the GPU-backed layered effect and
premultiplied alpha, while `WS_EX_TRANSPARENT` and `HTTRANSPARENT` provide full-window
click-through behavior when locked.
