const net = require('net');
const crypto = require('crypto');
const { spawn } = require('child_process');
const path = require('path');

console.log('Running Enhanced End-to-End WebSocket Integration Test...');

const runnerExe = path.join(__dirname, 'e2e_server_runner.exe');

const serverProcess = spawn(runnerExe, [], { stdio: ['pipe', 'pipe', 'pipe'] });

serverProcess.stderr.on('data', (d) => {
    console.error('Server Stderr: ' + d.toString());
});

serverProcess.stdout.on('data', (data) => {
    if (data.toString().includes('SERVER_READY')) {
        runClient();
    }
});

function runClient() {
    const socket = new net.Socket();
    const secKey = crypto.randomBytes(16).toString('base64');
    let messagesReceived = [];

    socket.connect(18890, '127.0.0.1', () => {
        const req = [
            'GET / HTTP/1.1',
            'Host: 127.0.0.1:18890',
            'Upgrade: websocket',
            'Connection: Upgrade',
            'Sec-WebSocket-Key: ' + secKey,
            'Sec-WebSocket-Version: 13',
            '',
            ''
        ].join('\r\n');
        socket.write(req);
    });

    let handshakeDone = false;
    let buffer = Buffer.alloc(0);

    socket.on('data', (chunk) => {
        buffer = Buffer.concat([buffer, chunk]);

        if (!handshakeDone) {
            const headerEnd = buffer.indexOf('\r\n\r\n');
            if (headerEnd !== -1) {
                const headerText = buffer.slice(0, headerEnd).toString();
                if (headerText.includes('101 Switching Protocols')) {
                    handshakeDone = true;
                    buffer = buffer.slice(headerEnd + 4);

                    // ส่งข้อความ Masked ui_state_change ไปยังเซิร์ฟเวอร์
                    sendMaskedText(socket, JSON.stringify({
                        action: 'ui_state_change',
                        type: 'mic',
                        state: true
                    }));
                } else {
                    throw new Error('Handshake failed: ' + headerText);
                }
            }
        }

        if (handshakeDone) {
            while (buffer.length >= 2) {
                const firstByte = buffer[0];
                const secondByte = buffer[1];
                const opcode = firstByte & 0x0f;
                let payloadLen = secondByte & 0x7f;
                let offset = 2;

                if (payloadLen === 126) {
                    if (buffer.length < 4) break;
                    payloadLen = buffer.readUInt16BE(2);
                    offset = 4;
                } else if (payloadLen === 127) {
                    if (buffer.length < 10) break;
                    payloadLen = Number(buffer.readBigUInt64BE(2));
                    offset = 10;
                }

                if (buffer.length < offset + payloadLen) {
                    break;
                }

                const payload = buffer.slice(offset, offset + payloadLen).toString('utf8');
                buffer = buffer.slice(offset + payloadLen);

                if (opcode === 1) { // Text frame
                    messagesReceived.push(JSON.parse(payload));
                }
            }
        }
    });

    socket.on('close', () => {
        verifyResults(messagesReceived);
    });

    socket.on('error', (err) => {
        if (err.code !== 'ECONNRESET') {
            console.error('Socket error:', err);
        }
    });
}

function sendMaskedText(socket, text) {
    const payload = Buffer.from(text, 'utf8');
    const mask = crypto.randomBytes(4);
    const frame = Buffer.alloc(2 + 4 + payload.length);

    frame[0] = 0x81; // FIN + text
    frame[1] = 0x80 | payload.length; // Masked
    mask.copy(frame, 2);

    for (let i = 0; i < payload.length; i++) {
        frame[6 + i] = payload[i] ^ mask[i % 4];
    }
    socket.write(frame);
}

function verifyResults(msgs) {
    console.log(`Received ${msgs.length} messages from C++ server:`, msgs);

    const expectedActions = [
        'sync_state',
        'set_mic',
        'set_audio'
    ];

    const actualActions = msgs.map(m => m.action);
    for (const expected of expectedActions) {
        if (!actualActions.includes(expected)) {
            console.error(`[FAILED] Missing expected action: ${expected}`);
            process.exit(1);
        }
    }

    // ตรวจสอบ Push-to-Talk messages (set_mic true และ false)
    const micStates = msgs.filter(m => m.action === 'set_mic').map(m => m.state);
    if (!micStates.includes(true) || !micStates.includes(false)) {
        console.error('[FAILED] Push-to-Talk state changes incomplete!');
        process.exit(1);
    }

    console.log('[PASSED] Enhanced End-to-End WebSocket Integration Test PASSED 100%!');
    process.exit(0);
}
