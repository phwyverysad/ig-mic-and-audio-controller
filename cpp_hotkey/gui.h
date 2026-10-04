#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "ws_server.h"

#define WM_TRAYICON (WM_APP + 101)
#define WM_CLIENT_COUNT_CHANGED (WM_APP + 102)
#define WM_KEY_RECORDED (WM_APP + 103)
#define ID_TRAY_SHOW 2001
#define ID_TRAY_EXIT 2002

class MainWindow {
public:
    MainWindow(HINSTANCE hInstance);
    ~MainWindow();

    bool Initialize(int nCmdShow);
    void UpdateClientCount(int count);
    void OnKeyRecorded(int target, const HotkeyBinding& binding);
    void RestoreWindow();
    void ExitApplication();

    HWND GetHwnd() const { return m_hWnd; }
    const AppConfig& GetConfig() const { return m_config; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void LoadConfigToUI();
    void SaveConfigFromUI();
    void UpdateKeyButtons();
    void InitSystemTray();
    void RemoveSystemTray();
    void ShowTrayMenu();
    void OnMicModeChanged();
    void OnAudioModeChanged();
    void OnScopeChanged();
    void OnBeepToggled();
    void OnAutoStartToggled();
    void OnInstallExtClicked();

    HINSTANCE m_hInstance;
    HWND m_hWnd;
    HFONT m_hFont;
    NOTIFYICONDATAW m_nid;

    AppConfig m_config;
    int m_connectedClients;

    // Controls HWNDs
    HWND m_btnMicKey;
    HWND m_cmbMicMode;
    HWND m_btnAudioKey;
    HWND m_cmbAudioMode;
    HWND m_cmbWindowScope;
    HWND m_chkBeep;
    HWND m_chkAutoStart;
    HWND m_lblStatus;
    HWND m_btnInstallExt;
    HWND m_btnSaveHide;

    bool m_recordingMic;
    bool m_recordingAudio;
};
