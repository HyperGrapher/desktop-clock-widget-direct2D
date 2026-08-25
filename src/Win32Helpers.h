#pragma once

#include <windows.h>

#include <cstdio>
#include <stdexcept>
#include <string>
#include <system_error>

namespace widget {

inline void CheckHr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        char code[32]{};
        std::snprintf(code, std::size(code), " (HRESULT 0x%08lX)",
                      static_cast<unsigned long>(result));
        throw std::runtime_error(std::string(operation) + code);
    }
}

inline void CheckWin32(BOOL result, const char* operation) {
    if (!result) {
        const DWORD code = GetLastError();
        throw std::system_error(static_cast<int>(code), std::system_category(), operation);
    }
}

inline int DipToPixels(float dip, UINT dpi) noexcept {
    return static_cast<int>((dip * static_cast<float>(dpi) / 96.0f) + 0.5f);
}

} // namespace widget
