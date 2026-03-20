# Thermostat Android

Android-приложение для проекта термостата, собранное по существующим файлам прошивки.

## На что я опирался

- `C:\Users\slobi\OneDrive\Documents\New project\TERMOSTAT\src\tx_main.c`
  Здесь есть текущая температура, режим установки уставки и диапазон `5..35 C`.
- `C:\Users\slobi\OneDrive\Documents\New project\TERMOSTAT\src\rx_main.c`
  Здесь видно правило включения нагрева: выход активен, когда `temp < setpoint`.
- `C:\Users\slobi\OneDrive\Documents\New project\esp32_ajax_test\main\main.c`
  Сейчас это тест локального интерфейса на ESP32, без сетевого API для телефона.

## Что внутри приложения

- экран текущей температуры;
- изменение уставки слайдером;
- индикация состояния нагрева;
- статус сопряжения;
- демо-режим для проверки интерфейса без железа;
- HTTP-слой под будущий шлюз `ESP32/Wi-Fi`.

## Важное ограничение

Телефон не может напрямую работать с радиообменом `nRF52832`, который реализован в прошивке сейчас.
Поэтому приложение сделано в двух режимах:

- `Demo` - локальная симуляция логики термостата;
- `Gateway` - работа через будущий HTTP-шлюз, например на ESP32.

## Ожидаемый API шлюза

`GET /api/thermostat`

```json
{
  "temperature": 23,
  "setpoint": 22,
  "heating": true,
  "paired": false,
  "status": "Receiver connected"
}
```

`POST /api/setpoint`

```json
{
  "setpoint": 24
}
```

`POST /api/pair`

Тело можно не передавать, ответ может вернуть то же состояние, что и `/api/thermostat`.

## Как открыть

1. Открой папку `D:\TERMOSTAT\ANDROID` в Android Studio.
2. Дай Studio скачать Gradle и Android SDK.
3. Собери `app`.

## Что ещё понадобится для реального телефона

- или добавить в ESP32-прошивку HTTP/BLE-шлюз;
- или сделать отдельный мост между радио `nRF52` и Wi-Fi/Bluetooth.
