# IG Audio Controller (Instagram Mic & Audio Controller)

Real-time microphone and incoming audio controller for Instagram WebRTC voice and video calls, powered by a native C++ Windows desktop application for global system hotkeys.

---

## 1. Key Features and Enhancements

1. **Push-to-Talk (PTT) Fully Functional**:
   - In Push-to-Talk mode, the microphone is muted by default.
   - Holding down the configured hotkey or mouse button unmutes the microphone immediately.
   - Releasing the key automatically mutes the microphone again without key-repeat stutter or interference from OS key-repeat events.
   - WebRTC audio tracks are controlled directly via track descriptor proxies without disconnecting or toggling the Instagram native in-call toolbar button.

2. **Bidirectional Real-Time State Synchronization**:
   - Pressing a hotkey instantly toggles the Instagram in-page floating status icon (White = Active, Red = Muted).
   - Clicking the in-page button with a mouse reflects back to the C++ controller and broadcasts to all open Instagram tabs in real time.
   - Seamlessly preserves state during Single Page Application (SPA) route changes without page reloads.
   - Newly opened tabs automatically synchronize state from the C++ controller upon connection.

3. **Universal Hotkey Support (Keyboard, Mouse, and Key Combinations)**:
   - **Mouse Buttons**: Supports Mouse 4 (XButton 1), Mouse 5 (XButton 2), Middle Mouse Button (Wheel), Left Click, and Right Click.
   - **Key Combinations**: Supports modifier combinations such as `Ctrl + Shift + M`, `Alt + F8`, `Shift + Mouse 4`, `Ctrl + Mouse 5`.
   - **Special Keys**: Numpad 0-9, Function keys F1-F24, Media Keys (Play/Pause, Mute, Next, Previous).

4. **Strict No-Emoji Architecture**:
   - Clean, formal interface without any emoji characters across GUI, System Tray, Tooltips, and In-Page overlays.

5. **Single Instance Guard**:
   - Uses a named Windows Mutex to prevent duplicate processes. Launching the executable again automatically restores and focuses the existing window.

6. **Auto-Start via Native Messaging**:
   - When navigating to Instagram in the browser, the extension automatically launches the C++ controller in the background.
   - Automatically minimizes to the Windows System Tray without stealing focus or interrupting other tasks.
   - Can be toggled on or off via the configuration GUI.

7. **Automatic Extension Installer**:
   - On startup, `IG Audio Controller.exe` automatically verifies whether the browser extension is installed.
   - If missing, it automatically clones or downloads the extension files from GitHub and launches Chrome to load it.
   - Includes a dedicated "Check / Install Chrome Extension" button in the GUI for one-click reinstallation.

---

## 2. Browser Extension Installation (Chrome / Edge / Brave)

1. Open your Chromium-based browser and navigate to:
   - Google Chrome: `chrome://extensions`
   - Microsoft Edge: `edge://extensions`
   - Brave: `brave://extensions`
2. Enable **Developer mode** in the top right corner.
3. Click **Load unpacked**.
4. Select the `extension` directory inside this project:
   `c:\Users\woran\Documents\My_Project\webstorebrowser\Instagram Mic & Audio Controller\extension`
5. The extension will activate and run automatically on `https://www.instagram.com`.

---

## 3. Using the C++ Controller (`IG Audio Controller.exe`)

1. Navigate to the `cpp_hotkey/` folder and launch `IG Audio Controller.exe`.
   - If the extension is not yet installed, the controller will detect this and automatically download the extension from GitHub.
   - You can also click the **Check / Install Chrome Extension** button at any time.
2. Configuration Options:
   - **Mic Hotkey**: Click `[ Click to record ]` and press any key, mouse button (e.g. Mouse 4), or combo (`Ctrl + Shift + M`).
   - **Mic Mode**:
     - `Toggle`: Press once to unmute, press again to mute.
     - `Push-to-Talk`: Muted by default; unmutes while holding the button.
   - **Audio Hotkey**: Click `[ Click to record ]` and press the desired key or mouse button.
   - **Audio Mode**:
     - `Toggle`: Press once to mute incoming sound, press again to unmute.
     - `Push-to-Mute`: Hold to temporarily mute incoming audio; release to restore sound.
   - **Execution Scope**:
     - `Global (All Windows)`: Hotkeys trigger from any window, desktop, or full-screen game.
     - `Browser & Instagram Only`: Hotkeys trigger only when the active window is a web browser.
   - **Beep Sound**: Play notification beeps on state transitions.
   - **Auto-Start with Instagram**: Automatically start controller when opening Instagram.
3. Click **Save and Run in Background (Hide Window)**:
   - The application minimizes to the Windows System Tray in the bottom-right taskbar.
   - Double-click the tray icon to restore the configuration window.
   - Right-click the tray icon and select Exit, or press `Ctrl + Alt + Q` for emergency shutdown.

---

## 4. Automated Testing Suite

To run all automated verification tests, open Command Prompt or PowerShell in the `tests/` directory:
```cmd
run_all_tests.bat
```
The test suite covers:
1. **No-Emoji Verification**: Validates 100% compliance with strict no-emoji requirements across all project files.
2. **Chrome Extension Verification**: Validates Manifest V3 structure, Service Worker, and dual content script execution.
3. **C++ Core Unit Tests**: Tests config persistence, key combinations, mouse mapping, and WebSocket RFC-6455 framing.
4. **End-to-End WebSocket Integration**: Verifies live client-server bidirectional state synchronization.
5. **Push-to-Talk and Mic Logic**: Tests WebRTC descriptor proxies and audio isolation.
6. **Real-time SPA Verification**: Verifies zero-refresh DOM injection and persistent communication channels.
7. **Extension Auto-Installer Verification**: Validates repository clone endpoints and fallback extraction logic.
