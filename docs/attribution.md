[简体中文](attribution.zh_CN.md) · English

# Attribution, dependencies and public export boundaries

## Source and art

- Upstream board support/template: [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport), MIT, © 2026 FoloToy. The upstream GitHub license endpoint was read and confirmed MIT on 2026-10-14.
- Life Countdown/Life Quest application, original TIPO/REED/monster sprites and geometric example avatar: © 2026 Lei Pan, MIT. Both FoloToy and Lei Pan notices remain in the root LICENSE.
- This is a sanitized export of source snapshot `1d2e42ba08b8136790afc8508949bc838a495c25`, not a copy of private Git history. Public changes use fictional names/role/birthday, replace the portrait with reproducible geometric art, widen its label, and retain only hardware-relevant tests/tools. No browser prototype or examination content is distributed.
- The photo `assets/photos/real-quests-1.3.jpg` is © Lei Pan, owner-supplied for project publishing. It is a historical physical device photo, not current-version acceptance. The source MIT license does not grant a separate blanket photo license; further image reuse requires the owner's permission. Only metadata markers were removed; the JPEG scan/pixels were retained. The final exact file was opened and inspected after stripping metadata.
- `assets/screenshots/public-example-home.png` is a same-source native render with fictional data, not a photo or real-device capture.

## Fonts

Noto Sans CJK SC Regular: © 2014–2021 Adobe, [official source](https://github.com/notofonts/noto-cjk/blob/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf), SIL OFL 1.1. Copyright and license were verified from the embedded OpenType name records. Original font and all generated `life_quest_font_{11,14,18}.c` subsets retain OFL; they are **not relicensed under MIT**. The subset names are distinct from the original font name. Full license is in `assets/fonts/OFL-NotoSansCJK.txt`. `quest-glyphs.json` records the source SHA-256, covered glyphs and pinned converter `lv_font_conv@1.5.3` (MIT build tool, not embedded). `tools/generate_quest_fonts.mjs` reproduces the subsets after that converter is cached with npm.

## Locked build dependencies

The actual downloaded component packages' license files were inspected. No vendor source from `managed_components/` or SDK is committed; the lockfile preserves versions and component hashes. Corresponding license copies are retained in `licenses/`.

| Dependency | Version | License |
| --- | --- | --- |
| ESP-IDF | 5.5.3, SDK commit `2c211b236707889e8400c4dc5644dd5c4ee071e0` | Apache-2.0 baseline; component-specific notices remain in SDK |
| espressif/button | 4.2.0 | Apache-2.0 |
| espressif/cmake_utilities | 1.1.1 | Apache-2.0 |
| espressif/esp_codec_dev | 1.6.2 | Apache-2.0 |
| espressif/esp_lvgl_port | 2.9.0 | Apache-2.0 |
| lvgl/lvgl | 9.5.0 | MIT, © 2025 LVGL Kft |

ESP-IDF contains third-party components under additional licenses; the SDK's own copyright and per-component license files remain authoritative. This release publishes this repository's source, not bundled SDK/toolchains, managed source or prebuilt firmware binaries. Anyone distributing a derived binary must also preserve the notices applicable to their actual linked SDK/components. Optional local tools: pyserial 3.5 (BSD-3-Clause), Pillow (HPND), and lv_font_conv (MIT); these are installed separately and are not firmware dependencies. Native rendering uses the same pinned LVGL/fonts as firmware.

## Population references and semantics

The frozen China five-year age-group table and world headline values in `main/life_quest_model.c` refer to July 2026, UN World Population Prospects 2024 medium projection. They were transcribed in the original project from [World Population Pyramid](https://worldpopulationpyramid.com/countries/china/), whose methodology attributes UN WPP. [UN WPP](https://population.un.org/wpp/) is the underlying statistical source. This export retains small factual numeric values, not website text, graphics, branding or a copied dataset file. Figures are a fixed approximate reference, not live counts or official individual ranks. China interpolation assumes uniform ages in a five-year band; the world view has no invented global percentile. The values are not newly refreshed during this export.

1 LT = 1 completed-calendar-day minute equivalent. Used LT = calendar days since birthday × 1440; remaining LT = max(0, calendar days to planning date) × 1440. It is not an AI tokenizer/billing metric, money or a health assessment.

## Privacy audit boundary

Excluded: private Git history, personal portraits, original birth information, examination images, publisher receipts, backups, serial logs, device identities, credentials, cloud/session storage, browser demo assets and old screenshots. `cardid` appears only as a protected partition contract, never as device content. The default fictional date is `2000-01-01`; saved personal profiles remain local on a user's device.

Three birthday-masked historical photos were rejected because remaining-life/used-LT values can disclose birthday indirectly. Only the task-page photograph is included; it has no birth-derived values or identity/QR. EXIF, Photoshop and comment metadata were removed. A textual secret/file scan supplements manual source/asset review; it is not proof that arbitrary future changes are safe. Run `python3 tools/audit_public_source.py` before publishing modifications and inspect every image separately.

The font generator uses the shipped glyph manifest by default. After changing UI characters, pass `--update-glyphs` and review regenerated fonts/manifest.
