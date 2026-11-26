#pragma once
namespace nativeime::core {
// A deliberately narrow experimental shortcut, not a global hotkey.
constexpr bool IsModeSwitchChord(bool f12,bool ctrl,bool shift,bool alt,bool win) noexcept {
    return f12&&ctrl&&shift&&!alt&&!win;
}
constexpr bool CanSwitchMode(bool writableContext,bool composing) noexcept {
    return writableContext&&!composing;
}
}
