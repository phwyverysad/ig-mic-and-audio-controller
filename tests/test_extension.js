const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

console.log('Testing Chrome Extension configuration, scripts, and bridge...');

const extDir = path.resolve(__dirname, '../extension');
const manifestPath = path.join(extDir, 'manifest.json');
const contentPath = path.join(extDir, 'content.js');
const bridgePath = path.join(extDir, 'bridge.js');
const bgPath = path.join(extDir, 'background.js');
const icon48Path = path.join(extDir, 'icons/icon48.png');
const icon128Path = path.join(extDir, 'icons/icon128.png');

// 1. Verify Manifest
if (!fs.existsSync(manifestPath)) {
    throw new Error('manifest.json does not exist!');
}
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
if (manifest.manifest_version !== 3) {
    throw new Error('manifest_version must be 3');
}
if (!manifest.background || manifest.background.service_worker !== 'background.js') {
    throw new Error('background service_worker missing in manifest.json');
}
if (!manifest.content_scripts || manifest.content_scripts.length < 2) {
    throw new Error('content_scripts missing bridge or content script in manifest.json');
}
console.log('  [PASS] manifest.json is valid Manifest V3 with Service Worker and Dual Content Scripts');

// 2. Verify Icons
if (!fs.existsSync(icon48Path) || fs.statSync(icon48Path).size === 0) {
    throw new Error('icon48.png is missing or empty');
}
if (!fs.existsSync(icon128Path) || fs.statSync(icon128Path).size === 0) {
    throw new Error('icon128.png is missing or empty');
}
console.log('  [PASS] Extension icons exist and are valid');

// 3. Syntax check content.js, bridge.js, background.js
try {
    execSync(`node -c "${contentPath}"`);
    execSync(`node -c "${bridgePath}"`);
    execSync(`node -c "${bgPath}"`);
    console.log('  [PASS] All JavaScript files have valid syntax');
} catch (e) {
    throw new Error('JavaScript syntax error: ' + e.message);
}

// 4. Content assertions
const contentJs = fs.readFileSync(contentPath, 'utf8');
const requiredTokens = [
    'getUserMedia',
    'RTCPeerConnection',
    'HTMLMediaElement.prototype.play',
    'toggle_mic',
    'set_mic',
    'toggle_audio',
    'set_audio',
    'sync_state',
    'config_update',
    'playBeep',
    'ig_hotkey_bridge',
    'isNativeMicButtonMuted',
    'syncInstagramNativeMicButton'
];

for (const token of requiredTokens) {
    if (!contentJs.includes(token)) {
        throw new Error(`content.js missing required feature: ${token}`);
    }
}

const bgJs = fs.readFileSync(bgPath, 'utf8');
const bgTokens = [
    'lastSyncState',
    'request_sync',
    'get_ws_status',
    'ensureControllerRunning'
];
for (const token of bgTokens) {
    if (!bgJs.includes(token)) {
        throw new Error(`background.js missing required feature: ${token}`);
    }
}

const bridgeJs = fs.readFileSync(bridgePath, 'utf8');
if (!bridgeJs.includes('response.sync_state')) {
    throw new Error('bridge.js missing sync_state forwarding');
}
console.log('  [PASS] content.js contains all required hooks and bridge communication');

console.log('[PASSED] All Chrome Extension tests passed successfully!');
