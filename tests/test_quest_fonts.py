#!/usr/bin/env python3
"""Verify shipped bitmap subsets cover every non-ASCII UI character."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
fonts = root / "assets/fonts"
manifest = json.loads((fonts / "quest-glyphs.json").read_text())
source = (fonts / manifest["source"]).read_bytes()
assert hashlib.sha256(source).hexdigest() == manifest["source_sha256"]
ui = "".join((root / path).read_text() for path in ("main/life_quest_app.c", "main/life_quest_ui.c"))
missing = {c for c in ui if ord(c) > 127} - set(manifest["symbols"])
assert not missing, "Regenerate bitmap fonts; missing glyphs: " + "".join(sorted(missing))
for size in (11, 14, 18):
    generated = (fonts / f"life_quest_font_{size}.c").read_text()
    assert '#include "lvgl.h"' in generated
    assert f"const lv_font_t life_quest_font_{size}" in generated
assert (fonts / "OFL-NotoSansCJK.txt").is_file()
print("Chinese font subset coverage/license: PASS")
