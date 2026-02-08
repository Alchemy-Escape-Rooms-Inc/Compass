# Compass Puzzle Controllers

Alchemy Escape Rooms - A Mermaid's Tale

Three compass props that each need to be rotated to a specific cardinal direction to solve their puzzle. All use the Watchtower Protocol for MQTT communication.

## Hardware

- **MCU**: ESP32-S3
- **Sensor**: Potentiometer (10K recommended) on GPIO 4
- **Power**: USB or 3.3V/5V supply

## Wiring (All Three Compasses)

| Component | ESP32-S3 Pin |
|-----------|-------------|
| Potentiometer Signal | GPIO 4 |
| Potentiometer VCC | 3.3V |
| Potentiometer GND | GND |

## Compass Solutions

| Compass | Target Direction | Degrees | MQTT Device Name |
|---------|-----------------|---------|-----------------|
| Blue Compass | NW (Northwest) | 315° | BlueCompass |
| Rose Compass | SE (Southeast) | 135° | RoseCompass |
| Silver Compass | NE (Northeast) | 45° | SilverCompass |

## MQTT Topics (Watchtower Protocol)

Each compass uses the pattern `MermaidsTale/{DeviceName}/...`:

| Topic | Purpose |
|-------|---------|
| `MermaidsTale/{Name}/command` | Receive commands |
| `MermaidsTale/{Name}/status` | Status updates & heartbeat |
| `MermaidsTale/{Name}/log` | Debug logs |
| `MermaidsTale/{Name}/direction` | Current angle (format: `pre_{angle}`) |
| `MermaidsTale/{Name}Solved` | Puzzle solved event (`triggered`) |

## Watchtower Commands

| Command | Response |
|---------|----------|
| `PING` | Returns `PONG` |
| `STATUS` | Returns JSON with device state |
| `RESET` | Reboots the ESP32-S3 |
| `PUZZLE_RESET` | Resets puzzle solved state |

## Build & Upload

Each compass is a separate PlatformIO project. Navigate to the compass folder and run:

```bash
cd BlueCompass
pio run --target upload
pio device monitor
```

## Cardinal Directions

```
        N (0°)
        |
 NW (315°)  NE (45°)
        |
W (270°) ---+--- E (90°)
        |
 SW (225°)  SE (135°)
        |
        S (180°)
```
