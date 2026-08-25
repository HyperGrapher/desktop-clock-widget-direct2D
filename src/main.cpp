#include "WidgetWindow.h"

#include <windows.h>

#include <winrt/base.h>

#include <exception>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        widget::WidgetWindow application(instance);
        application.Create();
        return application.RunMessageLoop();
    } catch (const std::exception& error) {
        MessageBoxA(nullptr, error.what(), "Desktop Clock Widget", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
}
