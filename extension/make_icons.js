const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

// ฟังก์ชันสร้างไฟล์ PNG อย่างง่ายโดยไม่ต้องใช้ไลบรารีภายนอก
function createPng(width, height, r, g, b, a) {
    const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

    // IHDR
    const ihdrData = Buffer.alloc(13);
    ihdrData.writeUInt32BE(width, 0);
    ihdrData.writeUInt32BE(height, 4);
    ihdrData.writeUInt8(8, 8); // 8-bit depth
    ihdrData.writeUInt8(6, 9); // RGBA
    ihdrData.writeUInt8(0, 10);
    ihdrData.writeUInt8(0, 11);
    ihdrData.writeUInt8(0, 12);

    const ihdrChunk = createChunk('IHDR', ihdrData);

    // IDAT
    // Raw scanlines: 1 filter byte (0) + width * 4 bytes
    const rowSize = 1 + width * 4;
    const rawData = Buffer.alloc(rowSize * height);

    for (let y = 0; y < height; y++) {
        const offset = y * rowSize;
        rawData.writeUInt8(0, offset); // Filter type 0
        for (let x = 0; x < width; x++) {
            const pxOffset = offset + 1 + x * 4;
            // Draw circle
            const cx = width / 2;
            const cy = height / 2;
            const dist = Math.sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            if (dist < width * 0.45) {
                rawData.writeUInt8(r, pxOffset);
                rawData.writeUInt8(g, pxOffset + 1);
                rawData.writeUInt8(b, pxOffset + 2);
                rawData.writeUInt8(a, pxOffset + 3);
            } else {
                rawData.writeUInt8(0, pxOffset);
                rawData.writeUInt8(0, pxOffset + 1);
                rawData.writeUInt8(0, pxOffset + 2);
                rawData.writeUInt8(0, pxOffset + 3);
            }
        }
    }

    const compressed = zlib.deflateSync(rawData);
    const idatChunk = createChunk('IDAT', compressed);

    // IEND
    const iendChunk = createChunk('IEND', Buffer.alloc(0));

    return Buffer.concat([signature, ihdrChunk, idatChunk, iendChunk]);
}

function createChunk(type, data) {
    const len = data.length;
    const buf = Buffer.alloc(4 + 4 + len + 4);
    buf.writeUInt32BE(len, 0);
    buf.write(type, 4, 4, 'ascii');
    data.copy(buf, 8);

    const crc = crc32(buf.slice(4, 8 + len));
    buf.writeUInt32BE(crc, 8 + len);
    return buf;
}

// CRC32 คำนวณ
const crcTable = [];
for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) {
        c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    }
    crcTable[n] = c >>> 0;
}

function crc32(buf) {
    let crc = 0xFFFFFFFF;
    for (let i = 0; i < buf.length; i++) {
        crc = (crc >>> 8) ^ crcTable[(crc ^ buf[i]) & 0xFF];
    }
    return (crc ^ 0xFFFFFFFF) >>> 0;
}

const iconsDir = path.join(__dirname, 'icons');
if (!fs.existsSync(iconsDir)) {
    fs.mkdirSync(iconsDir, { recursive: true });
}

// สร้างไอคอนสีม่วง/ชมพู Instagram (R: 225, G: 48, B: 108)
fs.writeFileSync(path.join(iconsDir, 'icon48.png'), createPng(48, 48, 225, 48, 108, 255));
fs.writeFileSync(path.join(iconsDir, 'icon128.png'), createPng(128, 128, 225, 48, 108, 255));
console.log('Icons created successfully.');
