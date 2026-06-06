# eSRA: Smart Room Automation Firmware

A smart room automation system built for **ESP32** that provides intelligent climate control and lighting automation with both BLE provisioning and a web-based control interface.

## Features

- **🌐 WiFi Provisioning via BLE**: Securely configure WiFi credentials using Bluetooth Low Energy
- **📱 Web Dashboard**: Real-time control panel accessible from any browser on the same network
- **🔄 Auto/Manual Modes**: Switch between automatic climate control and manual override
- **🌡️ Temperature Monitoring**: Real-time temperature sensing with configurable thresholds
- **💡 Light Detection**: LDR-based ambient light sensing for automatic lighting
- **🌀 Motor Control**: Soft-start fan control with smooth ramp-up to prevent power surges
- **📡 WebSocket Communication**: Low-latency real-time state updates
- **🔧 mDNS Support**: Access device at `esra.local` on your network

## Hardware Requirements

### Board
- **ESP32 Dev Module** (ESP32-D0WD-V3)

### Components
- **DHT11 Sensor** (Temperature & Humidity) - Pin 4
- **LDR Module** (Light Detection) - Pin 34
- **LED** - Pin 2
- **DC Motor** with flyback diode - Pin 5
- **Stabilized Power Supply** (5V for logic, sufficient amperage for motor)

### Pin Configuration
```
GPIO 2  - LED output
GPIO 4  - DHT11 data pin
GPIO 5  - Motor PWM (5 kHz, 8-bit)
GPIO 34 - LDR analog input
```

## Dependencies

Add these libraries via Arduino IDE or PlatformIO:

```
- Adafruit DHT Sensor Library
- WebSockets by Markus Sattler
- ArduinoJson (6.x or later)
- NimBLE-Arduino (ESP32)
```

## Installation & Setup

### 1. Flash the Firmware
1. Install [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/)
2. Select **ESP32 Dev Module** as board
3. Configure board settings:
   - CPU Frequency: 80MHz or higher
   - Flash Size: 4MB
   - Upload Speed: 115200
4. Install required libraries (Sketch → Include Library → Manage Libraries)
5. Upload `firmware.ino` to the ESP32

### 2. Initial Configuration (BLE Provisioning)

#### First Boot
- Device starts in BLE advertisement mode as **"eSRA_ESP32"**
- No WiFi credentials saved

#### Using Mobile App
1. Scan for BLE device "eSRA_ESP32"
2. Connect and obtain available WiFi networks
3. Send WiFi credentials (SSID + Password) as JSON:
   ```json
   {
     "ssid": "Your_WiFi_SSID",
     "pass": "Your_WiFi_Password"
   }
   ```
4. Device automatically connects and stores credentials

#### Serial Monitor
- Monitor connection progress at **115200 baud**
- Successful connection displays assigned IP address

## Web Dashboard

Once provisioned and connected to WiFi:

### Access Methods
- **IP Address**: Navigate to `http://<device_ip>/`
- **mDNS**: Open `http://esra.local/` (recommended)

### Dashboard Features
- **Live Sensors**: Real-time temperature and light status
- **Mode Toggle**: Switch between AUTO and MANUAL modes
- **Manual Controls** (MANUAL mode only):
  - Toggle LED on/off
  - Toggle Fan on/off
- **Auto Controls** (AUTO mode only):
  - LED automatically turns on in darkness
  - Fan activates when temperature exceeds threshold
  - Temperature threshold adjustment

## Operation Modes

### Auto Mode (Default)
- **Lighting**: LED automatically turns on when light level is low
- **Cooling**: Motor/fan activates when temperature exceeds configured threshold
- Manual device controls are disabled

### Manual Mode
- Full direct control of LED and fan
- Auto features disabled
- Use web dashboard buttons to toggle devices

## Configuration

### Temperature Threshold
- Default: **30°C**
- Adjust via web dashboard or edit code
- Persisted to device flash storage

### Sensor Reading Interval
- Default: **2000ms** (2 seconds)
- Modify `SENSOR_INTERVAL` constant in code

### Motor Soft-Start
- Ramps from 50/255 to 255/255 PWM
- Prevents power surge on motor startup
- 20ms increments for smooth acceleration

## API & Communication

### WebSocket Messages

#### Broadcast (Device → Client)
```json
{
  "temp": 28.5,
  "light": "Bright",
  "mode": "AUTO",
  "led": "OFF",
  "fan": "ON",
  "threshold": 30.0
}
```

#### Commands (Client → Device)
```json
{"command": "mode", "value": "toggle"}
{"command": "led", "value": "toggle"}
{"command": "fan", "value": "toggle"}
{"command": "threshold", "value": 32.0}
{"command": "reset", "value": null}
```

### BLE Characteristics

| UUID | Type | Purpose |
|------|------|---------|
| `beb5483e-36e1-4688-b7f5-ea07361b26a8` | WRITE | WiFi credentials (JSON) |
| `d991b58a-36b6-4f7f-8566-f3353549666c` | READ | WiFi scan results |
| `c77b4a2c-d64e-46ad-9720-6d8048f3f4e2` | READ | Device IP address |

## Factory Reset

Send a reset command via WebSocket to clear stored credentials and restart in BLE provisioning mode:

```json
{"command": "reset"}
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| **BLE not visible** | Restart device, check Bluetooth is enabled on phone |
| **WiFi connection fails** | Verify SSID/password, check WiFi signal strength |
| **Dashboard inaccessible** | Check device is on same network, verify IP address in serial monitor |
| **Motor not running** | Check flyback diode is installed, verify power supply amperage |
| **Temperature reads NaN** | Check DHT11 wiring and data pin connection |
| **Cannot resolve esra.local** | Some networks don't support mDNS; use IP address directly |

## Power Considerations

- **Logic & Sensors**: ~100mA typical
- **LED**: ~20mA at full brightness
- **Motor**: Varies (up to 2A+) - **use dedicated power supply**
- **Recommended**: 5V/3A minimum power supply

## Serial Debug Output

Enable serial monitor (115200 baud) to view:
- WiFi connection attempts
- BLE provisioning logs
- Sensor readings
- WebSocket activity
- System state changes

## Future Enhancements

- [ ] MQTT integration for Home Assistant
- [ ] Humidity-based ventilation control
- [ ] Scheduling system
- [ ] Energy consumption tracking
- [ ] Multi-device synchronization
- [ ] OTA firmware updates


## Support

For issues or feature requests, please refer to project documentation or contact the development team.
