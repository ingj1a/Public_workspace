#pragma once
#include <windows.h>
#include "core/input_preferences.hpp"
namespace nativeime::win {
inline constexpr wchar_t PreferenceKey[]=L"Software\\NativeIME\\Alpha";
inline core::InputPreferences LoadInputPreferences() noexcept {
    DWORD bits=1,size=sizeof(bits);
    if(RegGetValueW(HKEY_CURRENT_USER,PreferenceKey,L"InputPreferences",RRF_RT_REG_DWORD,nullptr,&bits,&size)!=ERROR_SUCCESS)
        return {};
    return core::DecodePreferences(bits);
}
inline bool SaveInputPreferences(core::InputPreferences p) noexcept {
    const DWORD bits=core::EncodePreferences(p);
    return RegSetKeyValueW(HKEY_CURRENT_USER,PreferenceKey,L"InputPreferences",REG_DWORD,&bits,sizeof(bits))==ERROR_SUCCESS;
}
}
