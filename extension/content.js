(function () {
    'use strict';

    if (window.__ig_mic_controller_injected) return;
    window.__ig_mic_controller_injected = true;

    // โหลดการตั้งค่าโหมดจาก localStorage ทันที เพื่อให้สถานะเริ่มต้นถูกต้องตั้งแต่เริ่มโหลด
    // เริ่มต้นเป็นโหมด Push-to-Talk (1) เพื่อให้ไมค์ปิด (false) ไว้อัตโนมัติเสมอ เว้นแต่ผู้ใช้จะตั้งเป็น Toggle (0)
    let savedMicMode = 1;
    try {
        const saved = localStorage.getItem('ig_mic_mode');
        if (saved !== null) {
            const parsed = parseInt(saved, 10);
            if (!isNaN(parsed)) savedMicMode = parsed;
        }
    } catch (e) {}

    let micMode = savedMicMode; // 0 = Toggle (สลับเปิด/ปิด), 1 = Push-to-Talk (กดค้างเพื่อพูด)
    // หากเป็นโหมด Push-to-Talk สถานะเริ่มต้นของไมค์จะต้องปิด (false) อัตโนมัติทันที
    let isMicActive = (micMode === 1) ? false : true;
    let isAudioActive = true;
    let isBeepEnabled = true;
    let audioMode = 0; // 0 = Toggle (สลับเปิด/ปิด), 1 = Push-to-Mute (กดค้างปิดเสียง)
    let isConnected = false;
    let isSyncingNativeButton = false;

    const activeMicTracks = new Set();
    let audioCtx = null;

    // สไตล์ CSS สำหรับไอคอน SVG และปุ่ม
    const styleEl = document.createElement('style');
    styleEl.textContent = `
        .ig-svg-icon {
            width: 22px !important;
            height: 22px !important;
            min-width: 22px !important;
            min-height: 22px !important;
            fill: none !important;
            stroke: currentColor !important;
            stroke-width: 2.2 !important;
            stroke-linecap: round !important;
            stroke-linejoin: round !important;
            display: block !important;
            pointer-events: none !important;
        }
        .ig-slash-line {
            stroke-width: 2.5 !important;
        }
        .ig-ctrl-btn {
            box-sizing: border-box !important;
            display: flex !important;
            align-items: center !important;
            justify-content: center !important;
            width: 36px !important;
            height: 36px !important;
            border-radius: 50% !important;
            cursor: pointer !important;
            transition: transform 0.15s ease, background-color 0.15s ease, filter 0.2s ease, color 0.2s ease !important;
            outline: none !important;
            background: transparent !important;
        }
        .ig-ctrl-btn:hover {
            transform: scale(1.1) !important;
            background-color: rgba(255, 255, 255, 0.12) !important;
        }
        .ig-ctrl-btn:active {
            transform: scale(0.95) !important;
        }
    `;
    (document.head || document.documentElement).appendChild(styleEl);

    // ระบบสังเคราะห์เสียง Beep ด้วย Web Audio API
    function getAudioContext() {
        if (!audioCtx) {
            const AudioContextClass = window.AudioContext || window.webkitAudioContext;
            if (AudioContextClass) {
                audioCtx = new AudioContextClass();
            }
        }
        if (audioCtx && audioCtx.state === 'suspended') {
            audioCtx.resume().catch(() => {});
        }
        return audioCtx;
    }

    function playBeep(isActive) {
        if (!isBeepEnabled) return;
        try {
            const ctx = getAudioContext();
            if (!ctx) return;

            const now = ctx.currentTime;
            if (isActive) {
                // เสียงสูง 1 ครั้งเมื่อเปิดใช้งาน (880 Hz)
                const osc = ctx.createOscillator();
                const gain = ctx.createGain();
                osc.type = 'sine';
                osc.frequency.setValueAtTime(880, now);
                gain.gain.setValueAtTime(0.08, now);
                gain.gain.exponentialRampToValueAtTime(0.001, now + 0.08);

                osc.connect(gain);
                gain.connect(ctx.destination);
                osc.start(now);
                osc.stop(now + 0.09);
            } else {
                // เสียงต่ำ 2 ครั้งเมื่อปิดใช้งาน/Mute (440 Hz)
                [0, 0.07].forEach((delay) => {
                    const osc = ctx.createOscillator();
                    const gain = ctx.createGain();
                    osc.type = 'sine';
                    osc.frequency.setValueAtTime(440, now + delay);
                    gain.gain.setValueAtTime(0.08, now + delay);
                    gain.gain.exponentialRampToValueAtTime(0.001, now + delay + 0.05);

                    osc.connect(gain);
                    gain.connect(ctx.destination);
                    osc.start(now + delay);
                    osc.stop(now + delay + 0.06);
                });
            }
        } catch (e) {}
    }

    // ==========================================
    // 1. ระบบควบคุมไมโครโฟน (WebRTC)
    // ==========================================
    const origEnabledDesc = (typeof MediaStreamTrack !== 'undefined' && MediaStreamTrack.prototype) ?
        (Object.getOwnPropertyDescriptor(MediaStreamTrack.prototype, 'enabled') ||
         Object.getOwnPropertyDescriptor(Object.getPrototypeOf(MediaStreamTrack.prototype), 'enabled')) : null;

    let isSettingHardware = false;

    function updateTrackHardwareState(track) {
        if (!track || track.kind !== 'audio') return;
        const desired = (track._igDesiredEnabled !== undefined) ? track._igDesiredEnabled : true;
        const actualState = isMicActive && (desired !== false);
        if (origEnabledDesc && origEnabledDesc.set) {
            try {
                isSettingHardware = true;
                origEnabledDesc.set.call(track, actualState);
            } catch (e) {
            } finally {
                isSettingHardware = false;
            }
        }
    }

    function registerMicTrack(track) {
        if (!track || track.kind !== 'audio') return;
        try {
            if (!activeMicTracks.has(track)) {
                activeMicTracks.add(track);
                if (track._igDesiredEnabled === undefined) {
                    track._igDesiredEnabled = true;
                }
                track.addEventListener('ended', () => {
                    activeMicTracks.delete(track);
                }, { once: true });
            }
            updateTrackHardwareState(track);
            // ควบคุมสถานะโดยตรงกรณีไม่มี descriptor proxy เพื่อป้องกัน recursion
            if (!isSettingHardware && (!origEnabledDesc || !origEnabledDesc.set)) {
                track.enabled = isMicActive;
            }
        } catch (e) {}
    }

    // Proxy MediaStreamTrack.prototype.enabled getter และ setter
    // เพื่อให้ Instagram เชื่อว่าไมค์ของ Instagram ยังเปิดอยู่เสมอ (ไมค์ instagram ไม่ต้องปิดไปด้วย)
    // ในขณะที่เสียงฮาร์ดแวร์จริงถูกปิด/เปิดตาม isMicActive ของเราอย่างแม่นยำ 100%
    if (typeof MediaStreamTrack !== 'undefined' && MediaStreamTrack.prototype && origEnabledDesc && origEnabledDesc.get && origEnabledDesc.set) {
        try {
            Object.defineProperty(MediaStreamTrack.prototype, 'enabled', {
                get: function () {
                    if (this.kind === 'audio') {
                        return (this._igDesiredEnabled !== undefined) ? this._igDesiredEnabled : true;
                    }
                    return origEnabledDesc.get.call(this);
                },
                set: function (val) {
                    if (this.kind === 'audio') {
                        if (isSettingHardware) return;
                        this._igDesiredEnabled = Boolean(val);
                        isSettingHardware = true;
                        try {
                            registerMicTrack(this);
                            updateTrackHardwareState(this);
                        } finally {
                            isSettingHardware = false;
                        }
                        return;
                    }
                    origEnabledDesc.set.call(this, val);
                },
                configurable: true,
                enumerable: true
            });
        } catch (e) {}
    }

    // Hook MediaStreamTrack.prototype.clone
    if (typeof MediaStreamTrack !== 'undefined' && MediaStreamTrack.prototype && MediaStreamTrack.prototype.clone) {
        const origTrackClone = MediaStreamTrack.prototype.clone;
        MediaStreamTrack.prototype.clone = function () {
            const cloned = origTrackClone.apply(this, arguments);
            if (cloned && cloned.kind === 'audio') {
                registerMicTrack(cloned);
            }
            return cloned;
        };
    }

    // Hook MediaStream.prototype.clone
    if (typeof MediaStream !== 'undefined' && MediaStream.prototype && MediaStream.prototype.clone) {
        const origStreamClone = MediaStream.prototype.clone;
        MediaStream.prototype.clone = function () {
            const clonedStream = origStreamClone.apply(this, arguments);
            if (clonedStream) {
                clonedStream.getAudioTracks().forEach(registerMicTrack);
            }
            return clonedStream;
        };
    }

    if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
        const originalGUM = navigator.mediaDevices.getUserMedia.bind(navigator.mediaDevices);
        navigator.mediaDevices.getUserMedia = async function (constraints) {
            const stream = await originalGUM(constraints);
            if (stream) {
                stream.getAudioTracks().forEach(track => {
                    registerMicTrack(track);
                });
            }
            return stream;
        };
    }

    if (window.RTCPeerConnection) {
        const originalAddTrack = RTCPeerConnection.prototype.addTrack;
        RTCPeerConnection.prototype.addTrack = function (track, ...streams) {
            if (track && track.kind === 'audio') {
                registerMicTrack(track);
            }
            return originalAddTrack.apply(this, [track, ...streams]);
        };

        if (RTCPeerConnection.prototype.addTransceiver) {
            const originalAddTransceiver = RTCPeerConnection.prototype.addTransceiver;
            RTCPeerConnection.prototype.addTransceiver = function (trackOrKind, init) {
                if (trackOrKind && typeof trackOrKind !== 'string' && trackOrKind.kind === 'audio') {
                    registerMicTrack(trackOrKind);
                }
                return originalAddTransceiver.apply(this, arguments);
            };
        }
    }

    if (window.RTCRtpSender && RTCRtpSender.prototype.replaceTrack) {
        const originalReplaceTrack = RTCRtpSender.prototype.replaceTrack;
        RTCRtpSender.prototype.replaceTrack = function (track) {
            if (track && track.kind === 'audio') {
                registerMicTrack(track);
            }
            return originalReplaceTrack.apply(this, arguments);
        };
    }

    if (window.MediaStream && MediaStream.prototype.addTrack) {
        const originalMediaStreamAddTrack = MediaStream.prototype.addTrack;
        MediaStream.prototype.addTrack = function (track) {
            if (track && track.kind === 'audio') {
                registerMicTrack(track);
            }
            return originalMediaStreamAddTrack.apply(this, arguments);
        };
    }

    // ตรวจสอบว่าปุ่มไมค์ของ Instagram ในหน้าโทร ปิดเสียงอยู่หรือไม่
    function isNativeMicButtonMuted(btn) {
        if (!btn) return false;
        const ariaLabel = (btn.getAttribute('aria-label') || '').toLowerCase();
        const title = (btn.getAttribute('title') || '').toLowerCase();
        let text = (ariaLabel + ' ' + title).trim();

        if (!text) {
            const svg = btn.querySelector('svg[aria-label]');
            if (svg) text = (svg.getAttribute('aria-label') || '').toLowerCase();
        }

        // หากป้ายกำกับมีคำว่า 'unmute', 'turn on', 'เปิด' แสดงว่าปัจจุบันไมค์ปิดอยู่
        if (text.includes('unmute') || text.includes('turn on') || text.includes('เปิด')) {
            return true;
        }
        // หากป้ายกำกับมีคำว่า 'turn off', 'mute', 'ปิด' แสดงว่าปัจจุบันไมค์เปิดอยู่
        if (text.includes('turn off') || text.includes('mute') || text.includes('ปิด')) {
            return false;
        }

        const pressed = btn.getAttribute('aria-pressed');
        if (pressed === 'true') return true;
        if (pressed === 'false') return false;

        return false;
    }

    // ซิงค์ปุ่มไมค์ในหน้าต่างโทรของ Instagram
    // แก้ไขตามข้อกำหนด: ปิดไมค์แบบไมค์ Instagram ไม่ต้องปิดไปด้วย
    // โดยควบคุมการตัดเสียงผ่าน WebRTC track.enabled โดยตรง ไม่คลิกปุ่มของ Instagram
    // เพื่อให้ไมค์ของ Instagram ไม่ต้องปิดไปด้วย และไม่เกิดอาการกระตุกหรือลูป
    function syncInstagramNativeMicButton(desiredActive) {
        // คงฟังก์ชันไว้เพื่อความเข้ากันได้ โดยไมค์ Instagram ไม่ต้องปิดไปด้วย
        return;
    }

    // นำสถานะไมโครโฟนไปใช้งานในทุกระบบ
    function applyMicState(active, triggerSound = true, notifyServer = false) {
        const stateChanged = (isMicActive !== active);
        isMicActive = active;

        // บังคับสถานะแทร็กเสียง WebRTC
        activeMicTracks.forEach(track => {
            if (track.readyState === 'live') {
                updateTrackHardwareState(track);
            } else {
                activeMicTracks.delete(track);
            }
        });

        // อัปเดตไอคอนบนหน้าเว็บทันทีแบบเรียลไทม์
        ensureUI();
        updateUI();

        // ซิงค์ปุ่มไมค์ของ Instagram
        syncInstagramNativeMicButton(isMicActive);

        if (stateChanged && triggerSound) {
            playBeep(isMicActive);
        }

        if (notifyServer) {
            sendToServer({
                action: 'ui_state_change',
                type: 'mic',
                state: isMicActive
            });
        }
    }

    function toggleMic() {
        applyMicState(!isMicActive, true, true);
    }

    // ==========================================
    // 2. ระบบควบคุมหูฟัง / ลำโพง (Incoming Audio)
    // ==========================================
    function setGlobalAudioState(enabled) {
        document.querySelectorAll('audio, video').forEach(media => {
            media.muted = !enabled;
        });
    }

    const originalPlay = HTMLMediaElement.prototype.play;
    HTMLMediaElement.prototype.play = function () {
        if (!isAudioActive) {
            this.muted = true;
        }
        return originalPlay.apply(this, arguments);
    };

    window.addEventListener('volumechange', (e) => {
        if (!isAudioActive && e.target && !e.target.muted) {
            e.target.muted = true;
        }
    }, true);

    function applyAudioState(active, triggerSound = true, notifyServer = false) {
        const stateChanged = (isAudioActive !== active);
        isAudioActive = active;

        setGlobalAudioState(isAudioActive);
        ensureUI();
        updateUI();

        if (stateChanged && triggerSound) {
            playBeep(isAudioActive);
        }

        if (notifyServer) {
            sendToServer({
                action: 'ui_state_change',
                type: 'audio',
                state: isAudioActive
            });
        }
    }

    function toggleAudio() {
        applyAudioState(!isAudioActive, true, true);
    }

    // สังเกตการณ์เมื่อมี Elements ใหม่ถูกเพิ่มเข้ามา (เช่น การโทรเริ่มต้นขึ้น)
    const domObserver = new MutationObserver(() => {
        if (!isAudioActive) {
            document.querySelectorAll('audio, video').forEach(el => {
                if (!el.muted) el.muted = true;
            });
        }
        syncInstagramNativeMicButton(isMicActive);
    });

    if (document.body) {
        domObserver.observe(document.body, { childList: true, subtree: true });
    } else {
        document.addEventListener('DOMContentLoaded', () => {
            domObserver.observe(document.body, { childList: true, subtree: true });
        });
    }

    // หมายเหตุ: ไมค์ของ Instagram ไม่ต้องปิดไปด้วย จึงไม่ดักจับคลิกเพื่อแย่งสถานะกับ Instagram
    // ทำให้ทั้งสองระบบทำงานร่วมกันได้อย่างราบรื่น ไม่เกิดปัญหาแย่งสถานะกันในโหมด Push-to-Talk

    // ==========================================
    // 3. ระบบสื่อสารกับ C++ (Bridge & Fallback)
    // ==========================================
    let hasBridge = false;
    let fallbackWs = null;

    function sendToServer(payload) {
        // ส่งผ่าน Extension Bridge
        window.postMessage({ source: 'ig_hotkey_page', data: payload }, '*');

        // หากต่อตรงผ่าน fallback WebSocket
        if (fallbackWs && fallbackWs.readyState === WebSocket.OPEN) {
            try {
                fallbackWs.send(JSON.stringify(payload));
            } catch (e) {}
        }
    }

    function handleServerMessage(data) {
        if (!data) return;

        if (data.action === 'ws_status' && typeof data.connected === 'boolean') {
            hasBridge = true;
            isConnected = data.connected;
            ensureUI();
            updateStatusIndicator();
            return;
        }

        isConnected = true;
        ensureUI();
        updateStatusIndicator();

        // ซิงค์สถานะครบชุด (Initial & Realtime State Synchronization)
        if (data.action === 'sync_state') {
            if (typeof data.mic_mode === 'number') {
                micMode = data.mic_mode;
                try { localStorage.setItem('ig_mic_mode', String(micMode)); } catch (e) {}
            }
            if (typeof data.audio_mode === 'number') {
                audioMode = data.audio_mode;
                try { localStorage.setItem('ig_audio_mode', String(audioMode)); } catch (e) {}
            }
            if (typeof data.beep_enabled === 'boolean') isBeepEnabled = data.beep_enabled;

            // ตรรกะ Push-to-Talk: หากโหมดไมค์เป็น Push-to-Talk (micMode === 1)
            // ไมค์จะต้องปิด (false) ทันที เว้นแต่ C++ จะระบุสถานะเป็น true ชัดเจน
            let targetMic = (typeof data.mic_active === 'boolean') ? data.mic_active : isMicActive;
            if (micMode === 1) {
                targetMic = (data.mic_active === true);
            }
            applyMicState(targetMic, false, false);

            let targetAudio = (typeof data.audio_active === 'boolean') ? data.audio_active : isAudioActive;
            if (audioMode === 1) {
                targetAudio = (data.audio_active !== false);
            }
            applyAudioState(targetAudio, false, false);
        } else if (data.action === 'toggle_mic') {
            if (micMode === 1) {
                applyMicState(!isMicActive, true, true);
            } else {
                toggleMic();
            }
        } else if (data.action === 'set_mic' && typeof data.state === 'boolean') {
            applyMicState(data.state, true, false);
        } else if (data.action === 'toggle_audio') {
            toggleAudio();
        } else if (data.action === 'set_audio' && typeof data.state === 'boolean') {
            applyAudioState(data.state, true, false);
        } else if (data.action === 'config_update') {
            if (typeof data.beep_enabled === 'boolean') {
                isBeepEnabled = data.beep_enabled;
            }
        } else if (data.action === 'ui_state_change') {
            if (data.type === 'mic' && typeof data.state === 'boolean') {
                applyMicState(data.state, true, false);
            } else if (data.type === 'audio' && typeof data.state === 'boolean') {
                applyAudioState(data.state, true, false);
            }
        }
    }

    // ฟังข้อความจาก bridge.js
    window.addEventListener('message', (event) => {
        if (event.source !== window || !event.data || event.data.source !== 'ig_hotkey_bridge') {
            return;
        }
        hasBridge = true;
        handleServerMessage(event.data.data);
    });

    // Fallback: หากไม่ได้รันผ่าน Extension ให้ต่อตรงที่ ws://localhost:18888
    setTimeout(() => {
        if (!hasBridge) {
            connectDirectFallback();
        }
    }, 1000);

    function connectDirectFallback() {
        if (hasBridge) return;
        try {
            fallbackWs = new WebSocket('ws://localhost:18888');

            fallbackWs.onopen = () => {
                isConnected = true;
                updateStatusIndicator();
                try {
                    fallbackWs.send(JSON.stringify({ action: 'request_sync' }));
                } catch (e) {}
            };

            fallbackWs.onmessage = (event) => {
                try {
                    handleServerMessage(JSON.parse(event.data));
                } catch (e) {}
            };

            fallbackWs.onclose = () => {
                isConnected = false;
                updateStatusIndicator();
                setTimeout(connectDirectFallback, 3000);
            };

            fallbackWs.onerror = () => {
                isConnected = false;
                updateStatusIndicator();
                try { fallbackWs.close(); } catch (e) {}
            };
        } catch (e) {
            isConnected = false;
            updateStatusIndicator();
            setTimeout(connectDirectFallback, 3000);
        }
    }

    // ==========================================
    // 4. UI บนหน้าเว็บ Instagram (ไม่มีอิโมจิ 100%)
    // ==========================================
    const ICONS = {
        micOn: '<svg viewBox="0 0 24 24" class="ig-svg-icon"><path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z"/><path d="M19 10v2a7 7 0 0 1-14 0v-2"/><line x1="12" y1="19" x2="12" y2="22"/><line x1="8" y1="22" x2="16" y2="22"/></svg>',
        micOff: '<svg viewBox="0 0 24 24" class="ig-svg-icon"><path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z"/><path d="M19 10v2a7 7 0 0 1-14 0v-2"/><line x1="12" y1="19" x2="12" y2="22"/><line x1="8" y1="22" x2="16" y2="22"/><line x1="3" y1="3" x2="21" y2="21" class="ig-slash-line"/></svg>',
        audioOn: '<svg viewBox="0 0 24 24" class="ig-svg-icon"><path d="M3 14h3a2 2 0 0 1 2 2v3a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-7a9 9 0 0 1 18 0v7a2 2 0 0 1-2 2h-1a2 2 0 0 1-2-2v-3a2 2 0 0 1 2-2h3"/></svg>',
        audioOff: '<svg viewBox="0 0 24 24" class="ig-svg-icon"><path d="M3 14h3a2 2 0 0 1 2 2v3a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-7a9 9 0 0 1 18 0v7a2 2 0 0 1-2 2h-1a2 2 0 0 1-2-2v-3a2 2 0 0 1 2-2h3"/><line x1="3" y1="3" x2="21" y2="21" class="ig-slash-line"/></svg>'
    };

    let micBtn, audioBtn, container, statusDot;
    let isDragging = false;
    let dragStartX = 0, dragStartY = 0;
    let initialContainerLeft = 0, initialContainerTop = 0;
    let hasMovedDuringDrag = false;

    function applySavedPosition() {
        if (!container) return;
        try {
            const savedPos = localStorage.getItem('ig_ctrl_pos');
            if (savedPos) {
                const pos = JSON.parse(savedPos);
                if (typeof pos.top === 'number' && typeof pos.left === 'number') {
                    const maxTop = Math.max(10, window.innerHeight - 120);
                    const maxLeft = Math.max(10, window.innerWidth - 60);
                    const top = Math.min(Math.max(10, pos.top), maxTop);
                    const left = Math.min(Math.max(10, pos.left), maxLeft);
                    container.style.top = top + 'px';
                    container.style.left = left + 'px';
                    container.style.bottom = 'auto';
                    container.style.right = 'auto';
                    return;
                }
            }
        } catch (e) {}

        container.style.bottom = '24px';
        container.style.right = '24px';
        container.style.top = 'auto';
        container.style.left = 'auto';
    }

    function createUI() {
        if (window !== window.top) return;
        if (!document.body) return;

        const existing = document.getElementById('ig-realtime-controller');
        if (existing) {
            container = existing;
            micBtn = document.getElementById('ig-btn-mic');
            audioBtn = document.getElementById('ig-btn-audio');
            statusDot = document.getElementById('ig-controller-status-dot');
            updateUI();
            updateStatusIndicator();
            return;
        }

        container = document.createElement('div');
        container.id = 'ig-realtime-controller';

        Object.assign(container.style, {
            position: 'fixed',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: '8px',
            zIndex: '2147483647',
            pointerEvents: 'auto',
            userSelect: 'none',
            background: 'rgba(20, 20, 24, 0.88)',
            backdropFilter: 'blur(16px)',
            webkitBackdropFilter: 'blur(16px)',
            padding: '8px 5px',
            borderRadius: '24px',
            boxShadow: '0 8px 32px rgba(0, 0, 0, 0.5), 0 0 0 1px rgba(255, 255, 255, 0.14)',
            touchAction: 'none'
        });

        applySavedPosition();

        // แดร็กเคลื่อนย้ายตำแหน่งได้เมื่อลากที่บริเวณตัวคอนโทรลเลอร์
        container.addEventListener('mousedown', (e) => {
            if (e.target.closest('#ig-btn-mic') || e.target.closest('#ig-btn-audio')) return;
            isDragging = true;
            hasMovedDuringDrag = false;
            dragStartX = e.clientX;
            dragStartY = e.clientY;
            const rect = container.getBoundingClientRect();
            initialContainerLeft = rect.left;
            initialContainerTop = rect.top;
            e.preventDefault();
        });

        // จุดแสดงสถานะการเชื่อมต่อ C++
        statusDot = document.createElement('div');
        statusDot.id = 'ig-controller-status-dot';
        Object.assign(statusDot.style, {
            width: '7px',
            height: '7px',
            borderRadius: '50%',
            backgroundColor: isConnected ? '#22c55e' : '#71717a',
            boxShadow: isConnected ? '0 0 6px #22c55e' : 'none',
            transition: 'background-color 0.25s ease, box-shadow 0.25s ease',
            cursor: 'grab',
            margin: '2px 0'
        });
        statusDot.title = isConnected ? 'สถานะ C++ Hotkey: เชื่อมต่อสำเร็จ' : 'สถานะ C++ Hotkey: ยังไม่ได้เชื่อมต่อ';

        // ปุ่มไมค์
        micBtn = document.createElement('div');
        micBtn.id = 'ig-btn-mic';
        micBtn.className = 'ig-ctrl-btn';

        let micWidgetHeld = false;
        micBtn.addEventListener('mousedown', (e) => {
            if (e.button !== 0) return;
            e.stopPropagation();
            if (micMode === 1) { // Push-to-Talk: กดค้างเพื่อพูด
                micWidgetHeld = true;
                applyMicState(true, true, true);
            }
        });

        window.addEventListener('mouseup', () => {
            if (micWidgetHeld) {
                micWidgetHeld = false;
                if (micMode === 1) {
                    applyMicState(false, true, true);
                }
            }
            if (isDragging) {
                isDragging = false;
                if (hasMovedDuringDrag && container) {
                    const rect = container.getBoundingClientRect();
                    try {
                        localStorage.setItem('ig_ctrl_pos', JSON.stringify({ top: rect.top, left: rect.left }));
                    } catch (err) {}
                }
            }
        });

        window.addEventListener('mousemove', (e) => {
            if (!isDragging || !container) return;
            const dx = e.clientX - dragStartX;
            const dy = e.clientY - dragStartY;
            if (Math.abs(dx) > 3 || Math.abs(dy) > 3) {
                hasMovedDuringDrag = true;
                const newLeft = Math.min(Math.max(10, initialContainerLeft + dx), window.innerWidth - 55);
                const newTop = Math.min(Math.max(10, initialContainerTop + dy), window.innerHeight - 110);
                container.style.left = newLeft + 'px';
                container.style.top = newTop + 'px';
                container.style.bottom = 'auto';
                container.style.right = 'auto';
            }
        });

        micBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            if (micMode === 0) { // Toggle: คลิกเพื่อสลับเปิด/ปิด
                toggleMic();
            }
        });

        // ปุ่มหูฟัง
        audioBtn = document.createElement('div');
        audioBtn.id = 'ig-btn-audio';
        audioBtn.className = 'ig-ctrl-btn';

        let audioWidgetHeld = false;
        audioBtn.addEventListener('mousedown', (e) => {
            if (e.button !== 0) return;
            e.stopPropagation();
            if (audioMode === 1) { // Push-to-Mute: กดค้างปิดเสียง
                audioWidgetHeld = true;
                applyAudioState(false, true, true);
            }
        });

        window.addEventListener('mouseup', () => {
            if (audioWidgetHeld) {
                audioWidgetHeld = false;
                if (audioMode === 1) {
                    applyAudioState(true, true, true);
                }
            }
        });

        audioBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            if (audioMode === 0) { // Toggle: คลิกเพื่อสลับเปิด/ปิดเสียง
                toggleAudio();
            }
        });

        container.appendChild(statusDot);
        container.appendChild(micBtn);
        container.appendChild(audioBtn);
        document.body.appendChild(container);

        updateUI();
        updateStatusIndicator();
    }

    function ensureUI() {
        if (window !== window.top) return;
        if (!document.body) return;

        if (!container || !document.getElementById('ig-realtime-controller')) {
            createUI();
        } else if (!document.body.contains(container)) {
            document.body.appendChild(container);
            updateUI();
            updateStatusIndicator();
        }
    }

    function setupButtonStyle(btn) {
        if (!btn) return;
        btn.className = 'ig-ctrl-btn';
    }

    function updateUI() {
        if (!micBtn || !audioBtn) return;

        micBtn.innerHTML = isMicActive ? ICONS.micOn : ICONS.micOff;
        micBtn.style.color = isMicActive ? '#ffffff' : '#ff4d4d';
        micBtn.style.filter = isMicActive ?
            'drop-shadow(0 0 6px rgba(255, 255, 255, 0.6))' :
            'drop-shadow(0 0 8px rgba(255, 77, 77, 0.8))';
        micBtn.title = isMicActive ?
            (micMode === 1 ? 'ไมโครโฟน: กำลังพูด (Push-to-Talk)' : 'ไมโครโฟน: เปิดอยู่') :
            (micMode === 1 ? 'ไมโครโฟน: ปิดอยู่ (กดปุ่มค้างเพื่อพูด)' : 'ไมโครโฟน: ปิดอยู่');

        audioBtn.innerHTML = isAudioActive ? ICONS.audioOn : ICONS.audioOff;
        audioBtn.style.color = isAudioActive ? '#ffffff' : '#ff4d4d';
        audioBtn.style.filter = isAudioActive ?
            'drop-shadow(0 0 6px rgba(255, 255, 255, 0.6))' :
            'drop-shadow(0 0 8px rgba(255, 77, 77, 0.8))';
        audioBtn.title = isAudioActive ? 'หูฟัง: เปิดเสียงอยู่' : 'หูฟัง: ปิดเสียงอยู่';
    }

    function updateStatusIndicator() {
        if (!statusDot) return;
        if (isConnected) {
            statusDot.style.backgroundColor = '#22c55e';
            statusDot.style.boxShadow = '0 0 6px #22c55e';
            statusDot.title = 'สถานะ C++ Hotkey: เชื่อมต่อสำเร็จ';
        } else {
            statusDot.style.backgroundColor = '#71717a';
            statusDot.style.boxShadow = 'none';
            statusDot.title = 'สถานะ C++ Hotkey: ยังไม่ได้เชื่อมต่อ (กรุณาเปิด IG Audio Controller.exe)';
        }
    }

    // เฝ้าสังเกตการณ์ DOM เพื่อคงคอนโทรลเลอร์ไว้ตลอดเวลาแบบเรียลไทม์ แม้จะเปลี่ยนหน้าใน Instagram (SPA)
    if (document.body) {
        const bodyObserver = new MutationObserver(() => ensureUI());
        bodyObserver.observe(document.body, { childList: true });
    } else {
        const rootObserver = new MutationObserver(() => {
            if (document.body) {
                rootObserver.disconnect();
                ensureUI();
                const bodyObs = new MutationObserver(() => ensureUI());
                bodyObs.observe(document.body, { childList: true });
            }
        });
        if (document.documentElement) {
            rootObserver.observe(document.documentElement, { childList: true });
        }
    }

    // ดักจับการเปลี่ยนหน้าของ React Router ใน Instagram ทันทีแบบเรียลไทม์
    try {
        const originalPush = history.pushState;
        history.pushState = function () {
            const result = originalPush.apply(this, arguments);
            setTimeout(ensureUI, 20);
            return result;
        };
        const originalReplace = history.replaceState;
        history.replaceState = function () {
            const result = originalReplace.apply(this, arguments);
            setTimeout(ensureUI, 20);
            return result;
        };
    } catch (e) {}

    window.addEventListener('popstate', () => setTimeout(ensureUI, 20));
    window.addEventListener('hashchange', () => setTimeout(ensureUI, 20));
    document.addEventListener('visibilitychange', () => {
        if (!document.hidden) ensureUI();
    });
    window.addEventListener('focus', () => ensureUI());

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', ensureUI, { once: true });
    } else {
        ensureUI();
    }

    // ตรวจสอบความต่อเนื่องอย่างสม่ำเสมอทุก 500ms
    setInterval(() => {
        ensureUI();
        if (!isConnected) {
            sendToServer({ action: 'get_ws_status' });
        }
    }, 500);

})();
