# ESPRoomba
Smartify your iRobot® using an ESP32-C3 running ESPHome

# Features
- Ability to wake the iRobot® up from normal & deep sleep
- Ability to command the iRobot® to start normal/spot/max cleaning/stop cleaning and go back to the dock
- Get a detailed charging status from the iRobot
- Ability to guess the iRobot®'s status (the [`Roomba® Open Interface (OI)`](./iRobot%20Roomba%20600%20Open%20Interface%20Spec.pdf) doesn't report cleaning status)
- Ability to report more advanced sensors (Battery temperature & voltage, General power usage and Motor currents)
- Ability to restart and power off the `iRobot®`
- Can be hooked into Home Assistant’s `Template Vacuum` configuration
- Broadcasts a Bluetooth iBeacon (can be used to track the iRobot®'s location using [ESPresense](https://espresense.com))

# Software Requirements
- Home Assistant
- ESPHome

# Hardware Requirements
- Compatible iRobot® Roomba®
- A WiFi network
- ESP32-C3 Super Mini
- Buck Converter (set it to `3.3v`)
- `2N3906` PNP Transistor

# Tested iRobot® Roomba®'s
- 600 series
- 700 series
- 800 series

# Credits
[mannkind](https://github.com/mannkind) - [Original implementation](https://github.com/mannkind/ESPHomeRoombaComponent)

[davidecavestro](https://github.com/davidecavestro) - [Native API support and a major rewrite](https://github.com/davidecavestro/ESPHomeRoombaComponent)

[wburgers](https://github.com/wburgers) - [Native UART support](https://github.com/wburgers/ESPHomeRoombaComponent)

[philpownall](https://github.com/philpownall) - [Manual controls, text display and dashboard config](https://github.com/philpownall/ESPHomeRoomba)