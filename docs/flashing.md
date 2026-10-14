[简体中文](flashing.zh_CN.md) · English

# Safe segmented flashing and recovery

These instructions are for the device owner. This public revision has not been installed or accepted on hardware. Confirm the exact badge, 8 MB Flash, stable power and current partition layout first; stop for other boards or layouts.

| Region | Offset | Size/limit | Policy |
| --- | --- | --- | --- |
| bootloader | `0x0` | built image length | validated segment only |
| partition table | `0x8000` | `0x1000` sector | MD5 and current-layout compatibility |
| NVS | `0x9000` | `0x6000` | preserve personal profile |
| PHY | `0xf000` | `0x1000` | preserve |
| application | `0x10000` | maximum `0x300000` | validated app only |
| cardid | `0x356000` | `0x4000` | unique per device; never overwrite/publish |

Never run `erase-flash`, full-chip erase or `--erase-all`, or flash a full-device backup. Never raw-write the merged image at `0x0`: its padding overwrites NVS at `0x9000`, even when it ends before cardid. Public artifacts must never include device-read cardid, NVS, full backups or serial logs.

## Prepare

Run `./tools/validate.sh --firmware` with IDF 5.5.3; require PASS. Keep the three segments and their SHA-256 hashes. Separately obtain your own known-working original segments with the same layout for rollback; do not install without reliable rollback material.

Replace PORT with your device's port. The following uses esptool 5.x from IDF 5.5.3. Private backups belong outside the repository and shared/cloud folders, for this same device only:

```sh
PORT='/dev/cu.YOUR_DEVICE'
esptool --chip esp32c3 --port "$PORT" flash-id
umask 077
PRIVATE_BACKUP_DIR="$HOME/ai-passport-private-backup"
mkdir -p "$PRIVATE_BACKUP_DIR"
esptool --chip esp32c3 --port "$PORT" read-flash 0x8000 0x1000 "$PRIVATE_BACKUP_DIR/partition-table.bin"
python "$IDF_PATH/components/partition_table/gen_esp32part.py" "$PRIVATE_BACKUP_DIR/partition-table.bin"
esptool --chip esp32c3 --port "$PORT" read-flash 0x9000 0x7000 "$PRIVATE_BACKUP_DIR/nvs-phy.bin"
esptool --chip esp32c3 --port "$PORT" read-flash 0x356000 0x4000 "$PRIVATE_BACKUP_DIR/cardid-private.bin"
chmod 600 "$PRIVATE_BACKUP_DIR"/*.bin
```

Verify that the existing NVS/PHY/application/cardid exactly match the table above. Stop and ask the manufacturer if they differ; do not replace the table experimentally. Never publish backups or use another device's identity.

## Write only validated segments

From this repository root, require the layout verification to pass, then:

```sh
python3 tools/verify_firmware.py build
esptool --chip esp32c3 --port "$PORT" write-flash \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/FoloToy-AI-Passport.bin
esptool --chip esp32c3 --port "$PORT" verify-flash \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/FoloToy-AI-Passport.bin
```

Check write hashes and verify-flash. Read NVS/PHY/cardid back privately and compare byte-for-byte with your pre-installation copies. Stop acceptance if differences are unexplained; do not upload those files. Validation does not need LQ_PROFILE or identity upload. Set the clock and test actual buttons, return/navigation, audio and five-minute sleep/wake. A build or simulated USB key input does not prove these checks.

## Recover

For interrupted writes, check power/cable/port and the esptool error; never erase everything to fix it. Enter download mode using manufacturer instructions (this project does not guess BOOT gestures) and rewrite the same validated three segments. If boot still fails, roll back using your own known-working original three segments with an identical partition layout, preserving NVS/PHY/cardid.

Never restore identity across devices or substitute example data. Missing backups, damaged identity, unknown layout or incompatible original firmware require the owner to contact FoloToy; this repository cannot regenerate vendor identity. Firmware hashes do not prove NVS migration compatibility, so keep personal backups before rollback.
