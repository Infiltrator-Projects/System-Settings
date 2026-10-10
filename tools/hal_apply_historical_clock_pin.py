#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")

replace_once(
    "CMakeLists.txt",
    'set(SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT\n    "7070c5812b50821fd7580101cb2289a3184f6b2c")',
    'set(SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT\n    "d880d90c5786887079920096b5ad41afca095c5d")',
)

Path("VERSION").write_text("0.4.66\n", encoding="utf-8")

changelog = Path("CHANGELOG.md")
text = changelog.read_text(encoding="utf-8")
entry = '''## 0.4.66 — 2026-10-10

- Consume Common's corrected document-style Traditional Chinese double-hour presentation: Earthly-Branch time now renders as forms such as `申時`, with native `初`/`正` half-shí precision when finer detail is enabled instead of pinyin/English zodiac teaching labels.
- Consume Common's native Roman daylight precision so `Show seconds` means the closest historically meaningful subdivision (`unciae`) of the current unequal `hora`, while the four-watch night convention remains coarse rather than inventing modern minutes.
- Keep clock/calendar policy ownership unchanged: System Settings selects the presentation and Common remains the single formatter used by participating applications.

'''
if text.startswith("## 0.4.66"):
    raise SystemExit("CHANGELOG already contains 0.4.66")
changelog.write_text(entry + text, encoding="utf-8")
