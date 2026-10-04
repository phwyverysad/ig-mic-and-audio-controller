// Background Service Worker (Manifest V3)
// ทำงานใน Extension Context โดยตรง ไม่ติดปัญหา Content Security Policy (CSP) ของหน้าเว็บ Instagram

let ws = null;
let reconnectTimer = null;
let isConnected = false;
let lastLaunchAttempt = 0;

// ลองเชื่อมต่อผ่าน 127.0.0.1 และ localhost
const WS_ENDPOINTS = [
    'ws://127.0.0.1:18888',
    'ws://localhost:18888'
];
let currentEndpointIndex = 0;

const connectedPorts = new Set();

function broadcastToInstagramTabs(message) {
    connectedPorts.forEach((port) => {
        try {
            port.postMessage(message);
        } catch (e) {
            connectedPorts.delete(port);
        }
    });

    chrome.tabs.query({ url: ['*://*.instagram.com/*', '*://instagram.com/*'] }, (tabs) => {
        if (!tabs || tabs.length === 0) return;
        tabs.forEach((tab) => {
            if (tab.id) {
                chrome.tabs.sendMessage(tab.id, message).catch(() => {});
            }
        });
    });
}

// สั่งเปิดโปรแกรม C++ อัตโนมัติผ่าน Chrome Native Messaging
function ensureControllerRunning() {
    if (isConnected) return;

    const now = Date.now();
    // ป้องกันการเรียกซ้ำถี่เกินไป (เว้นระยะอย่างน้อย 3 วินาที)
    if (now - lastLaunchAttempt < 3000) return;
    lastLaunchAttempt = now;

    try {
        chrome.runtime.sendNativeMessage('com.instagram.hotkey.controller', { action: 'launch' }, (response) => {
            if (chrome.runtime.lastError) {
                // หากยังไม่ได้ลงทะเบียน Native Host ให้ลองเชื่อมต่อ Socket ตามปกติ
                setTimeout(connectWebSocket, 1000);
            } else {
                setTimeout(connectWebSocket, 500);
            }
        });
    } catch (e) {
        setTimeout(connectWebSocket, 1000);
    }
}

// สถานะล่าสุดที่ซิงค์จาก C++ Controller
let lastSyncState = {
    action: 'sync_state',
    mic_active: false,
    audio_active: true,
    mic_mode: 1, // เริ่มต้นด้วยโหมด Push-to-Talk ปิดไมค์ไว้อัตโนมัติ
    audio_mode: 0,
    beep_enabled: true,
    auto_start_enabled: true
};

if (typeof chrome !== 'undefined' && chrome.storage && chrome.storage.local) {
    try {
        chrome.storage.local.get(['lastSyncState'], (result) => {
            if (result && result.lastSyncState) {
                Object.assign(lastSyncState, result.lastSyncState);
                if (lastSyncState.mic_mode === 1) {
                    lastSyncState.mic_active = false;
                }
            }
        });
    } catch (e) {}
}

function saveSyncState() {
    if (typeof chrome !== 'undefined' && chrome.storage && chrome.storage.local) {
        try {
            chrome.storage.local.set({ lastSyncState });
        } catch (e) {}
    }
}

function connectWebSocket() {
    if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
        return;
    }

    const endpoint = WS_ENDPOINTS[currentEndpointIndex];

    try {
        ws = new WebSocket(endpoint);

        ws.onopen = () => {
            isConnected = true;
            if (reconnectTimer) {
                clearTimeout(reconnectTimer);
                reconnectTimer = null;
            }
            broadcastToInstagramTabs({ action: 'ws_status', connected: true });
            // ขอสถานะล่าสุดจาก C++ ทันทีที่เชื่อมต่อ
            try {
                ws.send(JSON.stringify({ action: 'request_sync' }));
            } catch (e) {}
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                // อัปเดต Cache สถานะ
                if (data.action === 'sync_state') {
                    Object.assign(lastSyncState, data);
                    if (lastSyncState.mic_mode === 1) {
                        lastSyncState.mic_active = (data.mic_active === true);
                    }
                    saveSyncState();
                } else if (data.action === 'set_mic' && typeof data.state === 'boolean') {
                    lastSyncState.mic_active = data.state;
                    saveSyncState();
                } else if (data.action === 'set_audio' && typeof data.state === 'boolean') {
                    lastSyncState.audio_active = data.state;
                    saveSyncState();
                } else if (data.action === 'ui_state_change') {
                    if (data.type === 'mic' && typeof data.state === 'boolean') lastSyncState.mic_active = data.state;
                    if (data.type === 'audio' && typeof data.state === 'boolean') lastSyncState.audio_active = data.state;
                    saveSyncState();
                } else if (data.action === 'config_update' && typeof data.beep_enabled === 'boolean') {
                    lastSyncState.beep_enabled = data.beep_enabled;
                    saveSyncState();
                }

                broadcastToInstagramTabs(data);
            } catch (err) {}
        };

        ws.onclose = () => {
            isConnected = false;
            broadcastToInstagramTabs({ action: 'ws_status', connected: false });
            currentEndpointIndex = (currentEndpointIndex + 1) % WS_ENDPOINTS.length;
            scheduleReconnect();
        };

        ws.onerror = () => {
            isConnected = false;
            broadcastToInstagramTabs({ action: 'ws_status', connected: false });
            try {
                ws.close();
            } catch (e) {}
        };
    } catch (e) {
        isConnected = false;
        broadcastToInstagramTabs({ action: 'ws_status', connected: false });
        currentEndpointIndex = (currentEndpointIndex + 1) % WS_ENDPOINTS.length;
        scheduleReconnect();
    }
}

function scheduleReconnect() {
    if (!reconnectTimer) {
        reconnectTimer = setTimeout(() => {
            reconnectTimer = null;
            connectWebSocket();
        }, 2000);
    }
}

// จัดการการเชื่อมต่อผ่าน Port ระยะยาว (Persistent Connection) กับ bridge.js
chrome.runtime.onConnect.addListener((port) => {
    if (port.name === 'ig_hotkey_channel') {
        connectedPorts.add(port);

        if (!isConnected) {
            connectWebSocket();
            ensureControllerRunning();
        }

        // ส่งสถานะทันทีแบบเรียลไทม์
        port.postMessage({ action: 'ws_status', connected: isConnected });
        port.postMessage(lastSyncState);

        port.onMessage.addListener((message) => {
            if (!message) return;

            if (message.action === 'heartbeat') {
                return; // ข้อความรักษา Service Worker ให้ตื่นตัวอยู่เสมอ
            }

            if (message.action === 'get_ws_status') {
                if (!isConnected) {
                    connectWebSocket();
                    ensureControllerRunning();
                }
                port.postMessage({ action: 'ws_status', connected: isConnected });
                port.postMessage(lastSyncState);
                return;
            }

            if (message.action === 'request_sync') {
                if (isConnected && ws && ws.readyState === WebSocket.OPEN) {
                    try { ws.send(JSON.stringify({ action: 'request_sync' })); } catch (e) {}
                }
                port.postMessage({ action: 'ws_status', connected: isConnected });
                port.postMessage(lastSyncState);
                return;
            }

            if (ws && ws.readyState === WebSocket.OPEN) {
                try { ws.send(JSON.stringify(message)); } catch (e) {}
            }
        });

        port.onDisconnect.addListener(() => {
            connectedPorts.delete(port);
        });
    }
});

// รับข้อความแบบ One-off จาก Content Script
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message && message.action === 'get_ws_status') {
        if (!isConnected) {
            ensureControllerRunning();
        }
        sendResponse({ connected: isConnected, sync_state: lastSyncState });

        // ส่งสถานะ sync_state ไปยังแท็บที่เพิ่งเปิดใหม่ทันที
        if (sender && sender.tab && sender.tab.id) {
            chrome.tabs.sendMessage(sender.tab.id, lastSyncState).catch(() => {});
        }
        return true;
    }

    if (message && message.action === 'request_sync') {
        if (isConnected && ws && ws.readyState === WebSocket.OPEN) {
            try {
                ws.send(JSON.stringify({ action: 'request_sync' }));
            } catch (e) {}
        }
        sendResponse({ connected: isConnected, sync_state: lastSyncState });
        return true;
    }

    if (message && ws && ws.readyState === WebSocket.OPEN) {
        try {
            ws.send(JSON.stringify(message));
        } catch (e) {}
    }
});

// ฉีดสคริปต์อัตโนมัติไปยังแท็บ Instagram ที่เปิดค้างอยู่ โดยไม่ต้องรีเฟรชหน้าเว็บ
function injectIntoOpenTabs() {
    if (typeof chrome === 'undefined' || !chrome.tabs || !chrome.scripting) return;

    chrome.tabs.query({ url: ['*://*.instagram.com/*', '*://instagram.com/*'] }, (tabs) => {
        if (!tabs || tabs.length === 0) return;
        tabs.forEach((tab) => {
            if (!tab.id) return;
            chrome.scripting.executeScript({
                target: { tabId: tab.id, allFrames: true },
                files: ['bridge.js'],
                world: 'ISOLATED'
            }).catch(() => {});

            chrome.scripting.executeScript({
                target: { tabId: tab.id, allFrames: true },
                files: ['content.js'],
                world: 'MAIN'
            }).catch(() => {});
        });
    });
}

// ตรวจจับเมื่อผู้ใช้เปิดหรือสลับมาหน้า Instagram เพื่อสั่งเปิดโปรแกรม C++ อัตโนมัติทันที
chrome.tabs.onActivated.addListener(() => {
    chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
        if (tabs && tabs[0] && tabs[0].url && tabs[0].url.includes('instagram.com')) {
            ensureControllerRunning();
        }
    });
});

chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
    if (tab && tab.url && tab.url.includes('instagram.com')) {
        ensureControllerRunning();
        if (changeInfo.status === 'complete') {
            injectIntoOpenTabs();
        }
    }
});

// ตรวจสอบเมื่อ Service Worker เริ่มทำงาน หรือ Extension ติดตั้ง/อัปเดต
chrome.runtime.onInstalled.addListener(() => {
    injectIntoOpenTabs();
    connectWebSocket();
    ensureControllerRunning();
});

chrome.runtime.onStartup.addListener(() => {
    injectIntoOpenTabs();
    connectWebSocket();
    ensureControllerRunning();
});

chrome.tabs.query({ url: ['*://*.instagram.com/*', '*://instagram.com/*'] }, (tabs) => {
    if (tabs && tabs.length > 0) {
        injectIntoOpenTabs();
        ensureControllerRunning();
    }
});

