# iRobot Roomba 700
Smartify your iRobot using an ESP-01S

# Features
- Detailed user config file
- DHCP/Static IP
- OTA Updates
- Communicates over MQTT
- Ability to reconnect to WiFi & MQTT
- Ability to wake the iRobot up from normal & deep sleep
- Ability to command the iRobot to start cleaning / stop cleaning and go back to the dock
- Ability to set the iRobot's clock (automatically and manually using an MQTT command)
- Get a detailed charging status from the iRobot (if the `DEBUG` option is set to true)
- Ability to guess the iRobot's status (the [`Roomba Open Interface (OI)`](https://www.irobotweb.com/-/media/MainSite/PDFs/About/STEM/Create/iRobot_Roomba_600_Open_Interface_Spec.pdf) doesn't report cleaning status)
- Ability to report more advanced sensors (Battery temperature & voltage, General power usage, Motor currents and the value of the Virtual Wall sensor) (if the `SENSORS` option is set to `true`)
- Can be hooked into Home Assistant using only a `MQTT Vacuum` configuration

# Credits
[thehookup](https://github.com/thehookup) - Original idea and [implementation](https://github.com/thehookup/MQTT-Roomba-ESP01)

[danielraq](https://github.com/danielraq) - Finding the packets used by 700 series Roombas
