# iRobot Roomba 700
Smartify your iRobot using an ESP-01S

# Features
- Detailed user config file (rename `config.h.example` to `config.h`)
- `DHCP` and `Static IP` support
- OTA Updates
- Communicates over MQTT
- Ability to reconnect to WiFi & MQTT
- Ability to wake the iRobot up from normal & deep sleep
- Ability to command the iRobot to start normal/spot/max cleaning / stop cleaning and go back to the dock
- Ability to set the iRobot's clock (automatically and manually using an MQTT command) with DST support (Needs to be enabled in the config)
- Get a detailed charging status from the iRobot (if the `DEBUG` option is set to `true`)
- Ability to guess the iRobot's status (the [`Roomba Open Interface (OI)`](https://www.irobotweb.com/-/media/MainSite/PDFs/About/STEM/Create/iRobot_Roomba_600_Open_Interface_Spec.pdf) doesn't report cleaning status)
- Ability to report more advanced sensors (Battery temperature & voltage, General power usage, Motor currents, the value of the Virtual Wall sensor and which button is being pressed) (if the `SENSORS` option is set to `true`)
- Ability to restart the `ESP` and the `iRobot`
- Ability to power off the `iRobot`
- Can be hooked into Home Assistant using only a `Template Vacuum` configuration

# Software Requirements
- Arduino IDE
- MQTT Broker

# Hardware Requirements
- iRobot Roomba 700 series
- A WiFi network
- ESP-01S
- FTDI Flasher
- Buck Converter (set it to `3.3v`)
- `2N3906` PNP Transistor

# Connection Diagram
![Connection Diagram](https://github.com/ShonP40/iRobot-Roomba-700/blob/master/connection-diagram.jpg)

# Credits
[thehookup](https://github.com/thehookup) - Original idea and [implementation](https://github.com/thehookup/MQTT-Roomba-ESP01)

[danielraq](https://github.com/danielraq) - Finding the packets used by 700 series Roombas
