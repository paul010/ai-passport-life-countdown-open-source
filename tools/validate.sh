#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local test_dir

    python3 tools/audit_public_source.py

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_life_countdown_model.c main/life_countdown_model.c \
        -o "${test_dir}/test_life_countdown_model"
    "${test_dir}/test_life_countdown_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_life_quest_model.c main/life_quest_model.c main/life_countdown_model.c \
        -o "${test_dir}/test_life_quest_model"
    "${test_dir}/test_life_quest_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_life_quest_nav.c main/life_quest_nav.c \
        -o "${test_dir}/test_life_quest_nav"
    "${test_dir}/test_life_quest_nav"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_idle_power.c main/idle_power.c \
        -o "${test_dir}/test_idle_power"
    "${test_dir}/test_idle_power"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/include \
        tests/test_button_sample.c -o "${test_dir}/test_button_sample"
    "${test_dir}/test_button_sample"
    python3 tests/test_verify_firmware.py
    python3 tests/test_quest_fonts.py
    python3 tests/test_device_serial_lines.py
    python3 tests/test_device_ui_protocol.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    # Preserve segmented images for a provisioned-device flash. Writing the
    # merged padding would overwrite NVS between the table and application.
    mkdir -p "${repo_root}/build/bootloader" "${repo_root}/build/partition_table"
    install -m 0644 "${validation_build_dir}/FoloToy-AI-Passport.bin" "${repo_root}/build/"
    install -m 0644 "${validation_build_dir}/bootloader/bootloader.bin" "${repo_root}/build/bootloader/"
    install -m 0644 "${validation_build_dir}/partition_table/partition-table.bin" "${repo_root}/build/partition_table/"
    install -m 0644 "${validation_build_dir}/flash_args" "${repo_root}/build/"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
