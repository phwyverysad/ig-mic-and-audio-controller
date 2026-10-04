#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

void RegisterNativeHost();
void UnregisterNativeHost();
void SetAutoStartRegistered(bool enable);
bool IsNativeMessagingLaunch();
bool HandleNativeMessagingIO(bool isDuplicateInstance);
