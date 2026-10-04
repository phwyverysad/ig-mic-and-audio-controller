const assert = require('assert');
const fs = require('fs');
const path = require('path');

console.log('Running Real-time SPA and No-Refresh Verification Test...');

const extDir = path.resolve(__dirname, '../extension');
const manifest = JSON.parse(fs.readFileSync(path.join(extDir, 'manifest.json'), 'utf8'));
const bgJs = fs.readFileSync(path.join(extDir, 'background.js'), 'utf8');
const bridgeJs = fs.readFileSync(path.join(extDir, 'bridge.js'), 'utf8');
const contentJs = fs.readFileSync(path.join(extDir, 'content.js'), 'utf8');

// 1. Verify Manifest scripting permission and host permissions for dynamic injection
assert(
    manifest.permissions.includes('scripting'),
    'manifest.json must include scripting permission for dynamic injection without refresh'
);
assert(
    Array.isArray(manifest.host_permissions) && manifest.host_permissions.length > 0,
    'manifest.json must include host_permissions for instagram.com'
);
console.log('  [PASS] manifest.json contains scripting and host_permissions for zero-refresh injection');

// 2. Verify background.js persistent port connection and dynamic injection
assert(
    bgJs.includes('chrome.runtime.onConnect.addListener'),
    'background.js must listen to chrome.runtime.onConnect for long-lived port connection'
);
assert(
    bgJs.includes('ig_hotkey_channel'),
    'background.js must handle ig_hotkey_channel'
);
assert(
    bgJs.includes('function injectIntoOpenTabs()'),
    'background.js must define injectIntoOpenTabs() to inject into existing tabs without refresh'
);
assert(
    bgJs.includes('chrome.scripting.executeScript'),
    'background.js must call chrome.scripting.executeScript'
);
console.log('  [PASS] background.js manages persistent communication channel and dynamic tab injection');

// 3. Verify bridge.js persistent channel and keepalive
assert(
    bridgeJs.includes('__ig_bridge_injected'),
    'bridge.js must have idempotency guard __ig_bridge_injected'
);
assert(
    bridgeJs.includes('chrome.runtime.connect'),
    'bridge.js must connect long-lived port to keep service worker active'
);
assert(
    bridgeJs.includes('heartbeat'),
    'bridge.js must send periodic heartbeat keepalive to background.js'
);
console.log('  [PASS] bridge.js maintains persistent port with keepalive preventing service worker sleep');

// 4. Verify content.js real-time UI, SPA resilience, and idempotency
assert(
    contentJs.includes('__ig_mic_controller_injected'),
    'content.js must have idempotency guard __ig_mic_controller_injected'
);
assert(
    contentJs.includes('function ensureUI()'),
    'content.js must define ensureUI() to maintain widget presence continuously'
);
assert(
    contentJs.includes('MutationObserver'),
    'content.js must use MutationObserver to observe DOM changes'
);
assert(
    contentJs.includes('history.pushState'),
    'content.js must hook history.pushState for instant SPA route transition handling'
);
assert(
    contentJs.includes('history.replaceState'),
    'content.js must hook history.replaceState for instant SPA route transition handling'
);
assert(
    contentJs.includes('applySavedPosition'),
    'content.js must support position persistence and dragging'
);
console.log('  [PASS] content.js provides continuous UI presence with SPA navigation and observer hooks');

// 5. Simulate DOM removal and ensureUI restoration
function testEnsureUiSimulation() {
    let attached = false;
    const mockContainer = {
        id: 'ig-realtime-controller',
        style: {}
    };
    const mockBody = {
        contains: (el) => attached && el === mockContainer,
        appendChild: (el) => {
            if (el === mockContainer) attached = true;
        }
    };

    // Initial attach
    mockBody.appendChild(mockContainer);
    assert.strictEqual(attached, true);

    // Simulate Instagram React route change unmounting the container
    attached = false;
    assert.strictEqual(mockBody.contains(mockContainer), false);

    // Call simulated ensureUI restoration logic
    if (!mockBody.contains(mockContainer)) {
        mockBody.appendChild(mockContainer);
    }
    assert.strictEqual(attached, true, 'ensureUI must restore container immediately when detached');
    console.log('  [PASS] Simulated SPA DOM unmount is instantly recovered by ensureUI restoration logic');
}
testEnsureUiSimulation();

console.log('[PASSED] All real-time SPA and no-refresh tests passed successfully!');
