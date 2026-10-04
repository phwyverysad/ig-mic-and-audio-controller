#include "key_names.h"
#include <sstream>
#include <iomanip>

std::wstring GetKeyDisplayName(DWORD vk) {
    // Mouse Buttons
    switch (vk) {
        case VK_LBUTTON: return L"Mouse Left";
        case VK_RBUTTON: return L"Mouse Right";
        case VK_MBUTTON: return L"Mouse Middle";
        case VK_XBUTTON1: return L"Mouse 4";
        case VK_XBUTTON2: return L"Mouse 5";
    }

    // Function Keys
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }

    // Numpad 0-9
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return L"Num " + std::to_wstring(vk - VK_NUMPAD0);
    }

    // Letters A-Z
    if (vk >= 'A' && vk <= 'Z') {
        return std::wstring(1, (wchar_t)vk);
    }

    // Digits 0-9
    if (vk >= '0' && vk <= '9') {
        return std::wstring(1, (wchar_t)vk);
    }

    // Numpad operations
    switch (vk) {
        case VK_MULTIPLY: return L"Num *";
        case VK_ADD: return L"Num +";
        case VK_SEPARATOR: return L"Num Sep";
        case VK_SUBTRACT: return L"Num -";
        case VK_DECIMAL: return L"Num .";
        case VK_DIVIDE: return L"Num /";
        case VK_NUMLOCK: return L"Num Lock";

        // Media Keys
        case VK_VOLUME_MUTE: return L"Mute (Media)";
        case VK_VOLUME_DOWN: return L"Volume Down";
        case VK_VOLUME_UP: return L"Volume Up";
        case VK_MEDIA_NEXT_TRACK: return L"Media Next";
        case VK_MEDIA_PREV_TRACK: return L"Media Prev";
        case VK_MEDIA_STOP: return L"Media Stop";
        case VK_MEDIA_PLAY_PAUSE: return L"Media Play/Pause";

        // Special / Navigation Keys
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_TAB: return L"Tab";
        case VK_ESCAPE: return L"Esc";
        case VK_BACK: return L"Backspace";
        case VK_DELETE: return L"Delete";
        case VK_INSERT: return L"Insert";
        case VK_HOME: return L"Home";
        case VK_END: return L"End";
        case VK_PRIOR: return L"Page Up";
        case VK_NEXT: return L"Page Down";
        case VK_UP: return L"Up Arrow";
        case VK_DOWN: return L"Down Arrow";
        case VK_LEFT: return L"Left Arrow";
        case VK_RIGHT: return L"Right Arrow";
        case VK_CAPITAL: return L"Caps Lock";
        case VK_SCROLL: return L"Scroll Lock";
        case VK_PAUSE: return L"Pause";
        case VK_SNAPSHOT: return L"Print Screen";
        case VK_LWIN: return L"Left Win";
        case VK_RWIN: return L"Right Win";
        case VK_APPS: return L"App Key";

        // OEM Punctuation
        case VK_OEM_1: return L"; :";
        case VK_OEM_PLUS: return L"= +";
        case VK_OEM_COMMA: return L", <";
        case VK_OEM_MINUS: return L"- _";
        case VK_OEM_PERIOD: return L". >";
        case VK_OEM_2: return L"/ ?";
        case VK_OEM_3: return L"` ~";
        case VK_OEM_4: return L"[ {";
        case VK_OEM_5: return L"\\ |";
        case VK_OEM_6: return L"] }";
        case VK_OEM_7: return L"' \"";
    }

    // Try Windows GetKeyNameTextW
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (scanCode != 0) {
        LONG lParam = (scanCode << 16);
        wchar_t keyName[64] = { 0 };
        if (GetKeyNameTextW(lParam, keyName, 64) > 0) {
            return std::wstring(keyName);
        }
    }

    // Fallback: Hex code
    std::wstringstream ss;
    ss << L"Key [0x" << std::hex << std::uppercase << vk << L"]";
    return ss.str();
}

std::wstring GetBindingDisplayName(const HotkeyBinding& binding) {
    if (binding.vkCode == 0) {
        return L"ไม่ได้ตั้งค่า";
    }

    std::wstring result = L"";
    if (binding.ctrl) result += L"Ctrl + ";
    if (binding.alt) result += L"Alt + ";
    if (binding.shift) result += L"Shift + ";
    if (binding.win) result += L"Win + ";

    result += GetKeyDisplayName(binding.vkCode);
    return result;
}
