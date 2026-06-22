# Dino Jump Game – ESP32

Chrome dinosaur jump game on a 0.96" SSD1306 OLED display
controlled by a TTP223 capacitive touch sensor.

## Hardware
- ESP32 DevKit V1
- SSD1306 0.96" OLED (I2C)
- TTP223 Capacitive Touch Module
- Passive Buzzer (optional)

## Wiring
| Component     | ESP32 GPIO |
|---------------|------------|
| OLED SDA      | GPIO21     |
| OLED SCL      | GPIO22     |
| TTP223 SIG    | GPIO4      |
| Buzzer        | GPIO18     |

## Libraries
- Adafruit SSD1306
- Adafruit GFX

## How to Play
- TAP  = normal jump
- HOLD = high jump
- Avoid the cacti!
