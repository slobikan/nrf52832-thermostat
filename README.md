# Thermostat Monorepo

Unified repository prepared on 2026-03-20 from the latest local project trees.

## Contents

- `android/` - Android thermostat app copied from `D:\TERMOSTAT\ANDROID` without local SDK/download caches/build output.
- `esp32/` - ESP32 firmware copied from `D:\TERMOSTAT\ESP32 Proekt` without the `build/` directory.
- `nrf/` - latest mainline nRF sources (`RX`, `TX`, `radio`) copied from `D:\TERMOSTAT\thermostat2`.
- `thermostat2_TM1621B/` - separate TM1621B variant kept as its own folder, per request.
- `docs/CHAT_HANDOFF_ANDROID_TERMOSTAT.md` - handoff/context document copied into the repo for future chats and Linux migration.

## Notes

- This repo is intended to be portable to Linux and GitHub.
- Local machine-specific files and build output are ignored in the root `.gitignore`.
- The chosen mainline nRF source tree is `thermostat2` because its `src/` files were the newest local copies at the time this repo was assembled.

## Suggested next steps

1. Create the GitHub repository.
2. Initialize git in this folder and commit.
3. Push to GitHub.
4. On Linux, clone this repo and continue from `docs/CHAT_HANDOFF_ANDROID_TERMOSTAT.md`.
