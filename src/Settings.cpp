#include "Settings.h"

#include "Win32Helpers.h"

#include <shlobj.h>
#include <wrl/client.h>

#include <filesystem>

namespace widget {
namespace {

int ReadInteger(const std::wstring& path, const wchar_t* key, int fallback) {
    return static_cast<int>(GetPrivateProfileIntW(L"Widget", key, fallback, path.c_str()));
}

void WriteInteger(const std::wstring& path, const wchar_t* key, int value) {
    const std::wstring text = std::to_wstring(value);
    CheckWin32(WritePrivateProfileStringW(L"Widget", key, text.c_str(), path.c_str()),
               "WritePrivateProfileStringW");
}

} // namespace

SettingsStore::SettingsStore() {
    PWSTR localAppData = nullptr;
    CheckHr(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData),
            "SHGetKnownFolderPath");
    std::filesystem::path directory(localAppData);
    CoTaskMemFree(localAppData);
    directory /= L"DesktopClockWidget";
    std::filesystem::create_directories(directory);
    path_ = (directory / L"settings.ini").wstring();
}

WidgetSettings SettingsStore::Load() const {
    WidgetSettings result;
    wchar_t monitor[CCHDEVICENAME]{};
    GetPrivateProfileStringW(L"Widget", L"Monitor", L"", monitor,
                             static_cast<DWORD>(std::size(monitor)), path_.c_str());
    result.monitorDevice = monitor;
    result.offsetX = ReadInteger(path_, L"OffsetX", result.offsetX);
    result.offsetY = ReadInteger(path_, L"OffsetY", result.offsetY);
    result.locked = ReadInteger(path_, L"Locked", 1) != 0;
    result.alwaysOnTop = ReadInteger(path_, L"AlwaysOnTop", 0) != 0;
    return result;
}

void SettingsStore::Save(const WidgetSettings& settings) const {
    CheckWin32(WritePrivateProfileStringW(L"Widget", L"Monitor", settings.monitorDevice.c_str(),
                                          path_.c_str()),
               "WritePrivateProfileStringW");
    WriteInteger(path_, L"OffsetX", settings.offsetX);
    WriteInteger(path_, L"OffsetY", settings.offsetY);
    WriteInteger(path_, L"Locked", settings.locked ? 1 : 0);
    WriteInteger(path_, L"AlwaysOnTop", settings.alwaysOnTop ? 1 : 0);
}

} // namespace widget
