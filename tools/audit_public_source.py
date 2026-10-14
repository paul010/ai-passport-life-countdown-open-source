#!/usr/bin/env python3
"""Audit this export's text, local links and allowlisted binary assets; no uploads."""
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP = {'.git', 'build', 'managed_components', '__pycache__'}
BINARY = {'assets/fonts/NotoSansCJKsc-Regular.otf',
          'assets/photos/real-quests-1.3.jpg',
          'assets/screenshots/public-example-home.png'}
PATTERNS = {
    'GitHub token': r'(?:ghp_|github_pat_)[A-Za-z0-9_]{20,}',
    'OpenAI key': r'sk-(?:proj-)?[A-Za-z0-9_-]{20,}',
    'AWS access key': r'(?:AKIA|ASIA)[A-Z0-9]{16}',
    'private key': r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
    'device QR': r'https://ai-passport\.folotoy\.cn/trae/\?s=[^<\s&]+&k=[^<\s&]+',
    'credential assignment': r'(?i)(?:password|api_key|access_token|secret_key)\s*[:=]\s*["\'][^"\']{8,}["\']',
    'local user path': r'/Users/[^/\s]+/',
    'old personal presets': r'\b(?:DALEI|XIAOLEI)\b',
}
errors = []
text_count = 0
files = [p for p in ROOT.rglob('*') if p.is_file() and not set(p.relative_to(ROOT).parts) & SKIP]
for p in files:
    relative = p.relative_to(ROOT).as_posix()
    if p.is_symlink():
        errors.append(f'{relative}: symlink')
    if p.suffix.lower() in {'.bin', '.log', '.heic', '.zip', '.pem', '.p12'}:
        errors.append(f'{relative}: forbidden backup/artifact type')
    if relative in BINARY:
        print(f'Asset {relative}: SHA256 {hashlib.sha256(p.read_bytes()).hexdigest()}')
        continue
    try:
        text = p.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        errors.append(f'{relative}: unreviewed binary')
        continue
    text_count += 1
    for label, pattern in PATTERNS.items():
        if relative == 'tools/audit_public_source.py' and label in {'local user path', 'old personal presets'}:
            continue  # scanner definitions, not actual personal data
        if re.search(pattern, text):
            errors.append(f'{relative}: possible {label}')
    if p.suffix == '.md':
        for target in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', text):
            if target.startswith(('http:', 'https:', '#', 'mailto:')):
                continue
            dest = target.split('#', 1)[0]
            if dest and not (p.parent / dest).exists():
                errors.append(f'{relative}: missing local link {dest}')
        for target in re.findall(r'<img[^>]+src="([^"]+)"', text):
            if not (p.parent / target).is_file():
                errors.append(f'{relative}: missing image {target}')
    if re.search(r'(?m)^(<<<<<<< |=======\s*$|>>>>>>> )', text):
        errors.append(f'{relative}: merge marker')
if errors:
    raise SystemExit('\n'.join(errors))
print(f'Public text/file/link audit: PASS ({text_count} text files; {len(BINARY)} reviewed binary assets)')
print('Image content and metadata require separate manual inspection. No device was accessed.')
