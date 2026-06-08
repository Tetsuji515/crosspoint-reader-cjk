# SD Recovery Firmware

This document describes the SD recovery firmware for devices whose normal OTA path is unreliable but can still use `Install firmware from SD`.

## User flow

Users do not need Python, PlatformIO, esptool, or command-line tools.

1. Download `firmware.bin` from the [SD Recovery release](https://github.com/aBER0724/crosspoint-reader-cjk/releases/tag/sd-recovery).
2. Prepare the target firmware you want to flash.
3. Rename the target firmware to a name that starts with `firmware-` or `firmware_` and ends with `.bin`, for example:

```text
firmware-target.bin
firmware-v1.3.0.bin
firmware_1.3.0.bin
```

4. Copy the recovery firmware and target firmware to the SD card root:

```text
SD card root/
├── firmware.bin         # Recovery firmware
└── firmware-target.bin  # Target firmware
```

5. Insert the SD card.
6. Use the current firmware's `Install firmware from SD` action to install `firmware.bin`.
7. Reboot after the recovery firmware is flashed.
8. Recovery scans the SD card and installs the other firmware file automatically.
9. After the final firmware boots, remove `firmware.bin` and the target firmware from the SD card.

Keep only one target firmware file on the SD card to avoid flashing the wrong image. Do not name the target firmware `firmware.bin`; that name is reserved for the recovery firmware.

## Why this exists

Firmware 0.3.3 already accepts `/firmware.bin` from SD, but its OTA path has high memory pressure. The recovery firmware is intentionally small and SD-first. It avoids EPUB parsing, library scanning, WebServer, GitHub JSON parsing, and HTTPS OTA.

## Build commands

```bash
pio run -e recovery
```

The GitHub Actions workflow `.github/workflows/sd_recovery.yml` builds the recovery environment and publishes only one release asset:

```text
firmware.bin
```

## Compatibility

The recovery firmware is an ESP32-C3 app image and must remain smaller than the app partition size, `0x680000` bytes. It uses the existing partition table and writes only through the normal OTA app partition flow.

Firmware 0.3.3 scans `/firmware-sc.bin`, `/firmware-tc.bin`, and `/firmware.bin` on the SD card root. The recovery release uses root `firmware.bin` as the first-stage image to keep the user instructions simple.

After recovery boots, it scans the SD root and `/recovery-payload` for target firmware files whose names start with `firmware-` or `firmware_` and end with `.bin`. It deliberately ignores root `/firmware.bin`, because that file is the recovery firmware itself.

## Memory notes

Recovery avoids normal app subsystems and links a recovery-only entry point. It reuses `FirmwareInstaller`, which allocates one 4096-byte heap buffer during firmware writes and frees it immediately after use. The buffer is intentionally not placed on the stack because ESP32-C3 stack headroom is limited.

## Safety notes

The update process writes the new image to the inactive OTA slot first, then switches boot only after the image is finalized. If the first write fails early, the device should normally continue booting the previous firmware.

No firmware update is completely risk-free. Keep the battery charged and do not power off the device while it is flashing. If recovery itself cannot boot or boot metadata is corrupted during an interrupted update, hardware or ROM bootloader flashing may still be required.

## Limits

This recovery firmware cannot repair a damaged bootloader, damaged partition table, or a device whose SD update entry cannot run. Those cases require ROM bootloader flashing or hardware service recovery.
