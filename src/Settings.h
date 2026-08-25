#pragma once

#include <windows.h>

#include <string>

namespace widget {

struct WidgetSettings {
    std::wstring monitorDevice;
    int offsetX = 32;
    int offsetY = 32;
    bool locked = true;
    bool alwaysOnTop = false;
};

class SettingsStore {
public:
    SettingsStore();

    [[nodiscard]] WidgetSettings Load() const;
    void Save(const WidgetSettings& settings) const;
    [[nodiscard]] const std::wstring& Path() const noexcept { return path_; }

private:
    std::wstring path_;
};

} // namespace widget
