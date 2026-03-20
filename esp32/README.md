# AJAX KLIM ESP32

ESP32 project for air-conditioner control with:

- TFT 1.8" SPI display
- Home Assistant integration over MQTT
- Wi-Fi connectivity
- UART bridge reserved for the nRF receiver module
- starter architecture for future Ethernet fallback after module selection (`W5500` or `LAN8720`)

Current hardware mapping:

- TFT `BL -> GPIO32`
- TFT `CS -> GPIO33`
- TFT `DC -> GPIO25`
- TFT `RST -> GPIO26`
- TFT `MOSI -> GPIO27`
- TFT `SCK -> GPIO14`
- UART `RX -> GPIO16`
- UART `TX -> GPIO17`
- Button `UP -> GPIO4`
- Button `DOWN -> GPIO13`
- Button `MENU/OK -> GPIO18`
- Button `EXIT/BACK -> GPIO23`
- W5500 `MISO -> GPIO19`
- W5500 `CS -> GPIO21`
- W5500 `RST -> GPIO22`
- W5500 shares `MOSI -> GPIO27` and `SCK -> GPIO14` with TFT
- W5500 `INT` is not used, the driver works in polling mode to keep wiring simple

Current software status:

- TFT splash/logo screen is working
- MQTT discovery payloads for Home Assistant are generated
- climate mode and target temperature commands are subscribed over MQTT
- UART protocol hooks to the nRF receiver are already in place
- W5500 Ethernet support is added and configured as the primary network path
- Wi-Fi remains available as fallback if SSID/password are set

Current default settings live in `main/main.c` as fallback values, so the project builds even without `menuconfig`.
Later they can be moved fully to `menuconfig` after the base controller flow is finalized.

Build example on this PC:

`cmd /c "call C:\Users\slobi\.espressif\frameworks\esp-idf-v5.3\export.bat && idf.py -C D:\TERMOSTAT\ESP32P~1 -B D:\TERMOSTAT\ESP32_Proekt_build build"`
