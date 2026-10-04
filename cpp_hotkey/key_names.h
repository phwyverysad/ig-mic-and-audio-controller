#pragma once
#include <windows.h>
#include <string>
#include "config.h"

std::wstring GetKeyDisplayName(DWORD vk);
std::wstring GetBindingDisplayName(const HotkeyBinding& binding);
