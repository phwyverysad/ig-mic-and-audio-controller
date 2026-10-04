const fs = require('fs');
const path = require('path');
const assert = require('assert');

console.log('Running Extension Installer Logic Unit Test...');

// 1. ตรวจสอบว่าไฟล์ extension_installer.h และ extension_installer.cpp มีอยู่จริง
const hPath = path.resolve(__dirname, '../cpp_hotkey/extension_installer.h');
const cppPath = path.resolve(__dirname, '../cpp_hotkey/extension_installer.cpp');

assert(fs.existsSync(hPath), 'extension_installer.h must exist');
assert(fs.existsSync(cppPath), 'extension_installer.cpp must exist');

const cppContent = fs.readFileSync(cppPath, 'utf8');

// 2. ตรวจสอบว่า URL GitHub ในโค้ดถูกต้องตรงกับที่ผู้ใช้กำหนด 100%
const targetRepo = 'https://github.com/phwyverysad/ig-mic-and-audio-controller.git';
assert(cppContent.includes(targetRepo), 'Repository URL must match ' + targetRepo);

// 3. ตรวจสอบว่าโฟลเดอร์ extension ภายในโปรเจกต์มี manifest.json ครบถ้วน
const extManifest = path.resolve(__dirname, '../extension/manifest.json');
assert(fs.existsSync(extManifest), 'manifest.json must be present in extension directory');

// 4. ตรวจสอบคำสั่ง fallback zip download จาก GitHub main branch
const zipUrl = 'https://github.com/phwyverysad/ig-mic-and-audio-controller/archive/refs/heads/main.zip';
assert(cppContent.includes(zipUrl), 'Fallback zip URL must match ' + zipUrl);

console.log('  [PASS] extension_installer.h and extension_installer.cpp verified');
console.log('  [PASS] GitHub remote clone URL and fallback zip URL verified');
console.log('  [PASS] Extension folder and manifest structure verified');
console.log('[PASSED] Extension Installer test passed successfully!');
