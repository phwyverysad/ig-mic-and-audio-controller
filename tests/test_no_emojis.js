const fs = require('fs');
const path = require('path');

// Regex สำหรับดักจับ Emoji ใน Unicode
const emojiRegex = /[\u{1F600}-\u{1F64F}\u{1F300}-\u{1F5FF}\u{1F680}-\u{1F6FF}\u{1F1E0}-\u{1F1FF}\u{2600}-\u{26FF}\u{2700}-\u{27BF}\u{1F900}-\u{1F9FF}\u{1FA70}-\u{1FAFF}]/u;

function checkDir(dir) {
    let files = fs.readdirSync(dir);
    let violations = [];

    for (let file of files) {
        let fullPath = path.join(dir, file);
        let stat = fs.statSync(fullPath);

        if (stat.isDirectory()) {
            if (file !== 'icons' && file !== '.git') {
                violations = violations.concat(checkDir(fullPath));
            }
        } else if (/\.(cpp|h|json|js|bat|ini|txt)$/i.test(file)) {
            let content = fs.readFileSync(fullPath, 'utf8');
            let lines = content.split('\n');
            lines.forEach((line, index) => {
                if (emojiRegex.test(line)) {
                    violations.push({
                        file: fullPath,
                        line: index + 1,
                        text: line.trim()
                    });
                }
            });
        }
    }
    return violations;
}

const rootDir = path.resolve(__dirname, '..');
console.log('Checking repository for emojis in: ' + rootDir);
const violations = checkDir(rootDir);

if (violations.length === 0) {
    console.log('[PASSED] No emojis found anywhere in the codebase. Strict no-emoji rule satisfied.');
    process.exit(0);
} else {
    console.error('[FAILED] Found ' + violations.length + ' emojis:');
    violations.forEach(v => {
        console.error(`- ${v.file}:${v.line}: ${v.text}`);
    });
    process.exit(1);
}
