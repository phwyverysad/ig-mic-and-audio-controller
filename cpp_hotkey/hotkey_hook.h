#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <functional>
#include "config.h"

enum class RecordTarget {
    None,
    Mic,
    Audio
};

class HotkeyHook {
public:
    using KeyRecordedCallback = std::function<void(RecordTarget, const HotkeyBinding&)>;
    using EmergencyExitCallback = std::function<void()>;
    using BroadcastCallback = std::function<void(const std::string&)>;

    static bool Install(
        BroadcastCallback broadcastCb,
        EmergencyExitCallback emergencyExitCb,
        KeyRecordedCallback keyRecordedCb
    );
    static void Uninstall();

    static void UpdateConfig(const AppConfig& config);
    static void StartRecording(RecordTarget target);
    static void CancelRecording();
    static bool IsRecording();

    static void SetMicActive(bool active);
    static void SetAudioActive(bool active);
    static bool GetMicActive();
    static bool GetAudioActive();

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static bool IsTargetWindowActive();
    static bool MatchesBinding(const HotkeyBinding& binding, DWORD vk, bool isKeyDown);
    static void HandleInput(DWORD vk, bool isKeyDown);
};
