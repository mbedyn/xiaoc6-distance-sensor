# XIAO ESP32‑C6 Zigbee Distance Sensor (VL53L1X)

Custom Zigbee distance sensor based on [**Seeed Studio XIAO ESP32‑C6**](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/) and **VL53L1X ToF**.  
Urządzenie działa jako **Zigbee End Device**, integruje się z **Home Assistant (ZHA)** i raportuje dystans w milimetrach poprzez **custom cluster 0xFC00**.

## ✨ Funkcje

- Pomiar odległości w zakresie **0–1000 mm**
- Zawężona wiązka pomiarowa do **~15°** (ROI 8×8)
- Raportowanie Zigbee tylko przy zmianie > **10 mm**
- Custom Zigbee cluster **0xFC00**, atrybut `distance_mm`
- Kompatybilne z **Home Assistant ZHA**
- Endpoint zawiera:
  - Basic (0x0000)
  - Identify (0x0003)
  - Custom cluster (0xFC00)

## 🛠️ Wymagane komponenty

- Seeed Studio **XIAO ESP32‑C6**
- Czujnik ToF **VL53L1X**
- Home Assistant z integracją **ZHA**
- Zasilanie 5V / USB‑C

## 🔌 Połączenia

| VL53L1X | XIAO ESP32‑C6 |
|--------|----------------|
| VCC    | 3V3            |
| GND    | GND            |
| SDA    | D4 (GPIO4)     |
| SCL    | D5 (GPIO5)     |

## 📦 Instalacja (Arduino IDE)

1. Zainstaluj **ESP32 board package** od Espressif.
2. Wybierz:
   - Board: **XIAO ESP32C6**
   - Partition Scheme: **Zigbee 4MB with spiffs**
   - Upload speed: **921600**
3. Dodaj bibliotekę **VL53L1X** (Pololu).
4. Wgraj plik `distance_sensor.ino`.

## 🔗 Parowanie z Home Assistant (ZHA)

1. W Home Assistant:
   - Ustawienia → Urządzenia → Zigbee → **Add device**
2. Wybierz **Add via this device** (Connect ZBT‑1).
3. Urządzenie pojawi się jako:

