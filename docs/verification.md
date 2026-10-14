[简体中文](verification.zh_CN.md) · English

# Verification of 1.3.1-life-quest-public

Prepared on 2026-10-14. Final results are recorded below after running the public source gates. A compiled source release is distinct from device acceptance.

## Checks and reproduction

- `./tools/validate.sh --static`: source/file/link audit; six C host suites (pixel math, calendar, quest state, navigation, idle timing, checked ADC samples); firmware-layout parser negative tests; Chinese font coverage/license; serial framing and tagged-device-protocol unit tests. These host tests use mocks and never open a device.
- `./tools/validate.sh --firmware`: ESP-IDF 5.5.3 fresh build, merged image and partition verification, including valid table MD5, correct segment offsets, app ≤ 3 MB and no identity payload in protected cardid. No flash or serial access.
- Native rendering after managed components are available:

```sh
cmake -S tests/native -B /tmp/life-quest-render -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/life-quest-render -j 6
mkdir -p /tmp/life-quest-frames
/tmp/life-quest-render/life_quest_render /tmp/life-quest-frames
```

This checks the shared home renderer's visible label/image bounds, all nine focus choices, cleared tasks, alternate fictional leap-day profile and LVGL pool floor. The host permits 48 KiB for 64-bit pointers; firmware uses 32 KiB. Native images are illustrations, not device captures. To produce a PNG use separately installed Pillow to convert the emitted PPM. Do not claim that Home checks cover every actual-device page.

## Historical evidence boundaries

The original private project's 1.3.0 notes record segmented installation and USB diagnostic regression, with physical-button feel/audio/timed sleep still needing owner acceptance. The original 1.3.1 notes record a build/community submission and explicitly say no device was flashed. Neither record proves the modified `1.3.1-life-quest-public` passed device tests. Historical raw logs, private backups and publisher receipts are excluded.

Device tests: **NOT RUN**. No badge was flashed, reset or queried in this export task. Remaining checks: physical buttons and wake input, audio perception, battery readings, five-minute sleep/wake, NVS upgrade/rollback and real-device UI memory/navigation on this exact public revision. No claim of bit-identical builds across machines: SDK/tool versions and source/lockfiles make builds reproducible, but timestamps/paths can affect binary hashes.

## Final results from this export

Executed on 2026-10-14: source/secret/file/link audit PASS; six C host suites and Python tests PASS; 11 native frames and Home bounds/focus checks PASS; fresh ESP-IDF 5.5.3 firmware/protected-layout gate PASS. Device tests NOT RUN.

Application: 834096 bytes; merged: 899632 bytes. Merged SHA-256: `f8c78c23fa7ec6bc441e7937e9a79e7d0cb6a1164d38fe563881feef12397251`.

[Machine-readable evidence](build-evidence.json) records segment offsets, sizes and SHA-256. Binaries remain only in ignored local build output, not in the public repository.

Initial checks failed on an old birthday assertion and an omitted protocol tool module; the first build failed because the SDK download was incomplete. These were repaired, and the results above are from final reruns. The isolated SDK was completed at pinned commits; the owner’s original SDK was not changed. Historical browser tests were not run because this package excludes the browser prototype.
