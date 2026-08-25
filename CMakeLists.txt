cmake_minimum_required(VERSION 3.24)

project(DesktopClockWidget VERSION 1.0.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "DesktopClockWidget only supports Windows.")
endif()

add_executable(DesktopClockWidget WIN32
    src/main.cpp
    src/AcrylicHost.cpp
    src/AcrylicHost.h
    src/ClockMath.cpp
    src/ClockMath.h
    src/CompositionRenderer.cpp
    src/CompositionRenderer.h
    src/Settings.cpp
    src/Settings.h
    src/WidgetWindow.cpp
    src/WidgetWindow.h
    src/Win32Helpers.h
    resources/app.rc
    resources/app.manifest
)

target_compile_features(DesktopClockWidget PRIVATE cxx_std_20)
target_compile_definitions(DesktopClockWidget PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_options(DesktopClockWidget PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(DesktopClockWidget PRIVATE
    d2d1
    d3d11
    dcomp
    dwrite
    dwmapi
    dxgi
    ole32
    runtimeobject
    shell32
    shlwapi
    user32
    windowsapp
    "$(NuGetPackageRoot)microsoft.windowsappsdk.foundation/1.8.260222000/lib/native/x64/Microsoft.WindowsAppRuntime.lib"
    "$(NuGetPackageRoot)microsoft.windowsappsdk.foundation/1.8.260222000/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib"
)
target_link_options(DesktopClockWidget PRIVATE /MANIFEST:NO)

# The package supplies the documented Win32 Desktop Acrylic controller and
# automatically bootstraps the framework for this unpackaged application.
set_property(TARGET DesktopClockWidget PROPERTY
    VS_PACKAGE_REFERENCES
        "Microsoft.WindowsAppSDK.Foundation_1.8.260222000;Microsoft.WindowsAppSDK.Runtime_1.8.260317003;Microsoft.Windows.CppWinRT_2.0.250303.1")
set_property(TARGET DesktopClockWidget PROPERTY VS_GLOBAL_WindowsPackageType None)
set_property(TARGET DesktopClockWidget PROPERTY VS_GLOBAL_AppxPackage false)
set_property(TARGET DesktopClockWidget PROPERTY VS_GLOBAL_WindowsAppSDKSelfContained false)
set_property(TARGET DesktopClockWidget PROPERTY VS_GLOBAL_WindowsAppSdkAutoInitialize false)
set_property(TARGET DesktopClockWidget PROPERTY VS_GLOBAL_NuGetTargetMoniker "native,Version=v0.0")
set_property(TARGET DesktopClockWidget PROPERTY
    VS_GLOBAL_BaseIntermediateOutputPath "${CMAKE_BINARY_DIR}/obj/DesktopClockWidget/")

set_target_properties(DesktopClockWidget PROPERTIES
    OUTPUT_NAME "DesktopClockWidget"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DesktopClockWidget>"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(clock_math_tests
        tests/clock_math_tests.cpp
        src/ClockMath.cpp
    )
    target_compile_features(clock_math_tests PRIVATE cxx_std_20)
    target_compile_definitions(clock_math_tests PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(clock_math_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_include_directories(clock_math_tests PRIVATE src)
    set_property(TARGET clock_math_tests PROPERTY
        VS_GLOBAL_BaseIntermediateOutputPath "${CMAKE_BINARY_DIR}/obj/clock_math_tests/")
    add_test(NAME clock_math COMMAND clock_math_tests)
endif()
