[简体中文](README.zh_CN.md) · English

# Life Countdown / Life Quest for AI Passport

Turn a wearable badge into a personal time dashboard: view time since a fictional or locally configured birthday, time to a personal planning age, daily quests, XP and original pixel companions.

This is the **actual FoloToy AI Passport firmware source**, version `1.3.1-life-quest-public`. The original life-countdown model remains in `main/life_countdown_model.c`; Life Quest adds offline tasks, rewards, navigation and settings. This snapshot starts a new public history. It is not the iOS app or a browser simulation.

<p><img src="assets/screenshots/public-example-home.png" width="240" alt="Same-source native render with a fictional profile"><img src="assets/photos/real-quests-1.3.jpg" width="240" alt="Historical physical badge showing the task page"></p>

Left: native LVGL render of this public source, **not a device screenshot**. Right: owner's physical photograph of historical installed 1.3 UI; not proof that this public revision was flashed. The photo has no birthday/time-profile numbers or device identity, and EXIF/Photoshop metadata was removed.

## Hardware and scope

Only the FoloToy AI Passport baseline: ESP32-C3, 8 MB Flash, 240 × 320 display, three UP/DOWN/OK buttons, no PSRAM; ESP-IDF **5.5.3**. Pins and peripherals are defined in `components/bsp/include/bsp_pins.h`. Other boards/revisions are untested. This is an independent community project, not an official FoloToy release.

- Offline operation; no accounts, cloud services, model API keys or network setup.
- Fictional defaults: `EXAMPLE`, `MAKER`, birthday `2000-01-01`, planning age 75; alternative name `SAMPLE2`. Change `main/life_profile.h` before building. The original geometric avatar is reproducible with `python3 tools/generate_example_avatar.py`.
- Existing on-device NVS profiles take priority. Upgrading does not replace a saved birthday or XP. Never publish your NVS or screenshots containing personal time numbers.
- Planning age is a personal reference, not a lifespan prediction. 1 Life Token (LT) means one calendar-derived minute, not AI model tokens or money. Tasks earn XP but do not extend the planning date.

## Build and use

Install [ESP-IDF v5.5.3](https://github.com/espressif/esp-idf/tree/v5.5.3) outside this repository:

```sh
git clone --branch v5.5.3 --recursive https://github.com/espressif/esp-idf.git ../esp-idf-v5.5.3
cd ../esp-idf-v5.5.3
./install.sh esp32c3
. ./export.sh
cd ../ai-passport-life-countdown-open-source
./tools/validate.sh --static
./tools/validate.sh --firmware
```

A C compiler and Python 3 are needed for host tests. The IDF Python environment supplies build dependencies; optional serial tools require `pyserial==3.5`. `dependencies.lock` pins the component versions/hashes; do not update dependencies during reproduction. The firmware gate builds in a fresh temporary directory, validates partition offsets/MD5/app size and absence of identity payloads, then copies segmented outputs into ignored `build/`.

**Read [safe flashing and recovery](docs/flashing.md) before installing.** Never erase the whole flash, write merged padding to a provisioned badge, or overwrite `cardid` at `0x356000`.

First boot: set the date/time in Settings (China time), select Tasks, complete a real activity and then confirm it. UP/DOWN moves or edits; OK opens/confirms; long OK returns Home and cancels unfinished edits. Each task gives 20 XP; all three add 40 XP, and 100 XP gains a level. The fourth task row undoes the latest claim. TIPO can remove one monster HP per day without XP; REED is the alternative buddy. Life has Numbers/LT/China/World/Milestones. Settings edits the profile/clock, sound, motion and confirmed game reset. Five minutes without keys requests deep sleep; physical sleep/wake remains unverified for this revision. A complete power loss requires clock confirmation; deep sleep retains time.

[Verification](docs/verification.md) reports this revision separately from historical 1.3.0 device evidence. USB diagnostics can disclose your birthday through `LQ_PROFILE` and can change time/keys; use only explicitly on your own device. No serial or flash operation was run for this release.

## Licensing and maintenance

Source and original pixel art: [MIT](LICENSE), © 2026 FoloToy and Lei Pan; retain both notices. Based on [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport). Noto fonts/subsets remain SIL OFL 1.1; dependencies retain their own licenses. See [attribution and privacy boundaries](docs/attribution.md).

Maintained scope is this hardware baseline and offline firmware, not the historical browser prototype, AB-731 examination content, iOS application, personal firmware backups or vendor provisioning. Please report bugs with version, build error and reproduction steps; remove birthday, time-profile values, QR codes, identity, SSIDs and logs with private data. Hardware/button feel, audio, battery accuracy and timed sleep/wake require owner testing. No support commitment or compatibility guarantee for future hardware/SDK versions.
