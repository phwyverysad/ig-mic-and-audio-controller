// Content Bridge Script (ISOLATED world)
// ทำหน้าที่เป็นสะพานเชื่อมระหว่าง Background Service Worker กับ Main World (content.js)

(function () {
    'use strict';

    if (window.__ig_bridge_injected) return;
    window.__ig_bridge_injected = true;

    let port = null;
    let heartbeatInterval = null;

    function isContextValid() {
        return typeof chrome !== 'undefined' && !!chrome.runtime && !!chrome.runtime.id;
    }

    function connectPort() {
        if (!isContextValid()) return;
        try {
            port = chrome.runtime.connect({ name: 'ig_hotkey_channel' });

            port.onMessage.addListener((message) => {
                if (message) {
                    window.postMessage({ source: 'ig_hotkey_bridge', data: message }, '*');
                }
            });

            port.onDisconnect.addListener(() => {
                port = null;
                if (heartbeatInterval) {
                    clearInterval(heartbeatInterval);
                    heartbeatInterval = null;
                }
                if (!isContextValid()) return;
                setTimeout(connectPort, 1000);
            });

            // ส่งข้อความรักษาการเชื่อมต่อและดึงสถานะเริ่มต้นผ่าน Port
            port.postMessage({ action: 'get_ws_status' });

            if (heartbeatInterval) clearInterval(heartbeatInterval);
            heartbeatInterval = setInterval(() => {
                if (!isContextValid()) {
                    if (heartbeatInterval) clearInterval(heartbeatInterval);
                    heartbeatInterval = null;
                    return;
                }
                if (port) {
                    try {
                        port.postMessage({ action: 'heartbeat' });
                    } catch (e) {
                        if (heartbeatInterval) clearInterval(heartbeatInterval);
                        heartbeatInterval = null;
                    }
                }
            }, 20000);
        } catch (e) {
            if (isContextValid()) {
                setTimeout(connectPort, 2000);
            }
        }
    }

    connectPort();

    // 1. รับข้อความจาก background.js (Broadcast fallback) แล้วส่งต่อให้ content.js (Main World)
    try {
        chrome.runtime.onMessage.addListener((message) => {
            if (message) {
                window.postMessage({ source: 'ig_hotkey_bridge', data: message }, '*');
            }
        });
    } catch (e) {}

    // 2. รับข้อความจาก content.js (Main World) แล้วส่งต่อไปยัง background.js แบบเรียลไทม์
    window.addEventListener('message', (event) => {
        if (event.source !== window || !event.data || event.data.source !== 'ig_hotkey_page') {
            return;
        }

        if (!isContextValid()) return;

        const payload = event.data.data;
        if (payload) {
            try {
                if (port) {
                    port.postMessage(payload);
                    return;
                }
                chrome.runtime.sendMessage(payload).catch(() => {});
            } catch (e) {}
        }
    });

    // 3. ตรวจสอบสถานะการเชื่อมต่อเริ่มต้นจาก background.js
    if (isContextValid()) {
        try {
            chrome.runtime.sendMessage({ action: 'get_ws_status' }, (response) => {
                if (response && typeof response.connected === 'boolean') {
                    window.postMessage({
                        source: 'ig_hotkey_bridge',
                        data: { action: 'ws_status', connected: response.connected }
                    }, '*');
                    if (response.sync_state) {
                        window.postMessage({
                            source: 'ig_hotkey_bridge',
                            data: response.sync_state
                        }, '*');
                    }
                }
            });
        } catch (e) {}
    }

})();
