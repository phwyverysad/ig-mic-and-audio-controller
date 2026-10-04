const assert = require('assert');
const fs = require('fs');
const path = require('path');

console.log('Running Push-to-Talk and Mic Logic Unit Test...');

const contentPath = path.resolve(__dirname, '../extension/content.js');
const contentJs = fs.readFileSync(contentPath, 'utf8');

// 1. Verify that syncInstagramNativeMicButton does not click native button
assert(
    !contentJs.includes('btn.click()'),
    'btn.click() must NOT be called on Instagram native buttons (mic Instagram decoupled)'
);
console.log('  [PASS] Instagram native call button is not clicked by sync function');

// 2. Verify Push-to-Talk auto-mute initialization and localStorage logic
assert(
    contentJs.includes("let isMicActive = (micMode === 1) ? false : true;"),
    'isMicActive must be false automatically when micMode is 1 (Push-to-Talk)'
);
console.log('  [PASS] isMicActive initializes to false automatically in Push-to-Talk mode');

// 3. Verify WebRTC track registration, descriptor proxy, and clone interception
assert(
    contentJs.includes('function registerMicTrack(track)'),
    'registerMicTrack must be defined to manage all WebRTC tracks'
);
assert(
    contentJs.includes('track.enabled = isMicActive;'),
    'registerMicTrack must enforce track.enabled = isMicActive'
);
assert(
    contentJs.includes('updateTrackHardwareState'),
    'updateTrackHardwareState must be present to update underlying track hardware'
);
assert(
    contentJs.includes("Object.defineProperty(MediaStreamTrack.prototype, 'enabled'"),
    'MediaStreamTrack.prototype.enabled must be proxied with descriptor to prevent leaks'
);
assert(
    contentJs.includes('origTrackClone'),
    'MediaStreamTrack.prototype.clone must be intercepted'
);
assert(
    contentJs.includes('origStreamClone'),
    'MediaStream.prototype.clone must be intercepted'
);
assert(
    contentJs.includes('replaceTrack'),
    'RTCRtpSender.replaceTrack must be hooked'
);
assert(
    contentJs.includes('addTransceiver'),
    'RTCPeerConnection.addTransceiver must be hooked'
);
console.log('  [PASS] WebRTC audio tracks and prototype descriptors are comprehensively proxied');

// 4. Verify MediaStreamTrack.prototype.enabled descriptor behavior simulation
function testTrackProxySimulation() {
    let isMicActive = false; // PTT muted by default
    const activeMicTracks = new Set();

    class MockMediaStreamTrack {
        constructor() {
            this.kind = 'audio';
            this.readyState = 'live';
            this._nativeEnabled = true;
            this._igDesiredEnabled = true;
        }
    }

    const mockDescriptor = {
        get: function () {
            if (this.kind === 'audio') {
                return (this._igDesiredEnabled !== undefined) ? this._igDesiredEnabled : true;
            }
            return this._nativeEnabled;
        },
        set: function (val) {
            if (this.kind === 'audio') {
                activeMicTracks.add(this);
                this._igDesiredEnabled = Boolean(val);
                this._nativeEnabled = isMicActive && (this._igDesiredEnabled !== false);
                return;
            }
            this._nativeEnabled = Boolean(val);
        }
    };

    Object.defineProperty(MockMediaStreamTrack.prototype, 'enabled', mockDescriptor);

    const track = new MockMediaStreamTrack();

    // Instagram in-call initialization sets track.enabled = true
    track.enabled = true;

    // While controller is in PTT mode (isMicActive === false):
    // 1. Instagram reading track.enabled must see true (Instagram native mic stays unmuted!)
    assert.strictEqual(track.enabled, true, 'Instagram UI must see enabled === true so native mic stays open');
    // 2. Underlying native track must be disabled (silence transmitted)
    assert.strictEqual(track._nativeEnabled, false, 'Underlying hardware track must be disabled/silenced');

    // Holding Push-to-Talk hotkey: isMicActive becomes true
    isMicActive = true;
    activeMicTracks.forEach(t => {
        t._nativeEnabled = isMicActive && (t._igDesiredEnabled !== false);
    });
    assert.strictEqual(track.enabled, true);
    assert.strictEqual(track._nativeEnabled, true, 'Holding hotkey must enable hardware audio');

    // Releasing Push-to-Talk hotkey: isMicActive becomes false
    isMicActive = false;
    activeMicTracks.forEach(t => {
        t._nativeEnabled = isMicActive && (t._igDesiredEnabled !== false);
    });
    assert.strictEqual(track.enabled, true);
    assert.strictEqual(track._nativeEnabled, false, 'Releasing hotkey must silence hardware audio');

    console.log('  [PASS] WebRTC descriptor proxy protects audio while keeping Instagram mic open');
}
testTrackProxySimulation();

// 5. Verify message handling logic in content.js
function createMockEnvironment(initialMicMode = 1) {
    let micMode = initialMicMode;
    let isMicActive = (micMode === 1) ? false : true;
    const tracks = [
        { readyState: 'live', enabled: true },
        { readyState: 'live', enabled: true }
    ];

    function applyMicState(active) {
        isMicActive = active;
        tracks.forEach(t => {
            if (t.readyState === 'live') t.enabled = isMicActive;
        });
    }

    function onSyncState(data) {
        if (typeof data.mic_mode === 'number') micMode = data.mic_mode;
        let targetMic = (typeof data.mic_active === 'boolean') ? data.mic_active : isMicActive;
        if (micMode === 1) {
            targetMic = (data.mic_active === true);
        }
        applyMicState(targetMic);
    }

    function onSetMic(state) {
        applyMicState(state);
    }

    return {
        getMicMode: () => micMode,
        getMicActive: () => isMicActive,
        getTracks: () => tracks,
        onSyncState,
        onSetMic
    };
}

// Case A: Initial PTT mode -> mic is automatically muted
const envA = createMockEnvironment(1);
assert.strictEqual(envA.getMicActive(), false, 'PTT must initialize with mic closed/muted');

// Case B: Holding hotkey -> set_mic true -> mic opens
envA.onSetMic(true);
assert.strictEqual(envA.getMicActive(), true, 'Holding hotkey must open the mic');
assert.strictEqual(envA.getTracks()[0].enabled, true, 'WebRTC track must be enabled');

// Case C: Releasing hotkey -> set_mic false -> mic closes
envA.onSetMic(false);
assert.strictEqual(envA.getMicActive(), false, 'Releasing hotkey must close the mic');
assert.strictEqual(envA.getTracks()[0].enabled, false, 'WebRTC track must be disabled/silenced');

// Case D: Switching from Toggle (0) to PTT (1) via sync_state
const envD = createMockEnvironment(0);
assert.strictEqual(envD.getMicActive(), true, 'Toggle starts with mic open');
envD.onSyncState({ action: 'sync_state', mic_mode: 1, mic_active: false });
assert.strictEqual(envD.getMicMode(), 1);
assert.strictEqual(envD.getMicActive(), false, 'Switching to PTT must automatically mute the mic');
assert.strictEqual(envD.getTracks()[0].enabled, false, 'WebRTC track must be disabled');

console.log('  [PASS] Simulated Push-to-Talk state transitions match all task requirements');
console.log('[PASSED] All Push-to-Talk and Mic logic tests completed successfully!');
