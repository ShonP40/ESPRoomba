#include "esphome.h"

#define ROOMBA_READ_TIMEOUT 200

class RoombaComponent : public UARTDevice, public CustomAPIDevice, public PollingComponent { 
	public:
		//Sensor *distanceSensor;
		Sensor *voltageSensor;
		Sensor *currentSensor;
		Sensor *batteryChargeSensor;
		Sensor *batteryCapacitySensor;
		Sensor *batteryPercentSensor;
		Sensor *batteryTemperatureSensor;
		TextSensor *chargingSensor;
		TextSensor *activitySensor;
		Sensor *driveSpeedSensor;
		TextSensor *oiModeSensor;
		Sensor *rightMotorCurrentSensor;
		Sensor *leftMotorCurrentSensor;
        Sensor *mainBrushCurrentSensor;
        Sensor *sideBrushCurrentSensor; 
        BinarySensor *vacuumSensor;
		BinarySensor *virtualWallSensor;
		BinarySensor *chargingSourcesSensor;

		static RoombaComponent* instance(uint8_t brcPin, UARTComponent *parent, uint32_t updateInterval, bool lazy650Enabled) {
			static RoombaComponent* INSTANCE = new RoombaComponent(brcPin, parent, updateInterval, lazy650Enabled);
			return INSTANCE;
		}

		void setup() override {
			if (this->lazy650Enabled) {
				// High-impedence on the BRC_PIN
				// see https://github.com/johnboiles/esp-roomba-mqtt/commit/fa9af14376f740f366a9ecf4cb59dec2419deeb0#diff-34d21af3c614ea3cee120df276c9c4ae95053830d7f1d3deaf009a4625409ad2R140
				pinMode(this->brcPin, INPUT);
			} else {
				pinMode(this->brcPin, OUTPUT);
				digitalWrite(this->brcPin, HIGH);
			}

			register_service(&RoombaComponent::on_command, "command", {"command"});
		}

    	void update() override {
			if (this->lazy650Enabled) {
				long now = millis();
				// Wakeup the roomba at fixed intervals
				if (now - lastWakeupTime > 50000) {
					ESP_LOGD("roomba", "Time to wakeup");
					lastWakeupTime = now;
					if (!wasCleaning) {
						if (wasDocked) {
							wake_on_dock();
						} else {
							brc_wakeup();
						}
					} else {
						brc_wakeup();
					}
				}
			}

			uint8_t charging;
			uint16_t voltage;
			int16_t current;
			uint16_t batteryCharge;
			uint16_t batteryCapacity;
			int16_t batteryTemperature;
			int16_t rightMotorCurrent;
			int16_t leftMotorCurrent;
            int16_t mainBrushCurrent;
            int16_t sideBrushCurrent;
			uint8_t virtualWall;
			uint8_t chargingSources;

			flush();

			uint8_t sensors[] = {
				SensorChargingState,
				SensorVoltage,
				SensorCurrent,
				SensorBatteryCharge,
				SensorBatteryCapacity,
				SensorBatteryTemperature,
				SensorOIMode,
				SensorRightMotorCurrent,
				SensorLeftMotorCurrent,
                SensorMainBrushCurrent,
                SensorSideBrushCurrent,
				SensorVirtualWall,
				SensorChargingSourcesAvailable,
			};

			uint8_t values[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

			bool success = getSensorsList(sensors, sizeof(sensors), values, sizeof(values));
			if (!success) {
				ESP_LOGD("roomba", "Could not get sensor values from serial");
				return;
			}

			charging = values[0];
			voltage = values[1] * 256 + values[2];
			current = values[3] * 256 + values[4];
			batteryCharge = values[5] * 256 + values[6];
      		batteryCapacity = values[7] * 256 + values[8];
			batteryTemperature = values[9];
			std::string oiMode = get_oimode(values[10]);
			rightMotorCurrent = values[11] * 256 + values[12]; 
			leftMotorCurrent = values[13] * 256 + values[14]; 
            mainBrushCurrent = values[15] * 256 + values[16];
            sideBrushCurrent = values[17] * 256 + values[18];
			virtualWall = values[19];
			chargingSources = values[20];

			std::string activity = get_activity(charging, current);
			wasCleaning = activity == "Cleaning";
			wasDocked = activity == "Docked";

			float voltageData = 0.001 * roundf(voltage * 100) / 100;
			if (this->voltageSensor->state != voltageData) {
				this->voltageSensor->publish_state(voltageData);
			}

			float currentData = 0.001 * roundf(current * 100) / 100;
			if (this->currentSensor->state != currentData) {
				this->currentSensor->publish_state(currentData);
			}

			float charge = 0.001 * roundf(batteryCharge * 100) / 100;
			if (this->batteryChargeSensor->state != charge) {
				this->batteryChargeSensor->publish_state(charge);
			}

			float capacity = 0.001 * roundf(batteryCapacity * 100) / 100;
			if (this->batteryCapacitySensor->state != capacity) {
				this->batteryCapacitySensor->publish_state(capacity);
			}

			float battery_level = 100.0 * ((1.0 * charge) / (1.0 * capacity));
			if (this->batteryPercentSensor->state != battery_level) {
				this->batteryPercentSensor->publish_state(battery_level);
			}

			if (this->batteryTemperatureSensor->state != batteryTemperature) {
				this->batteryTemperatureSensor->publish_state(batteryTemperature);
			}

			if (this->chargingState != charging) {
				this->chargingState = charging;
				this->chargingSensor->publish_state(ToString(charging));
			}

			if (activity.compare(this->activitySensor->state) != 0) {
				this->activitySensor->publish_state(activity);
			}

			if (this->driveSpeedSensor->state != this->speed) {
				this->driveSpeedSensor->publish_state(this->speed);
			}

			if (oiMode.compare(this->oiModeSensor->state) != 0) {
				this->oiModeSensor->publish_state(oiMode);
			}

			float rightMotorCurrentData = 0.001 * (rightMotorCurrent * 100) / 100;
            if(this->rightMotorCurrentSensor->state != rightMotorCurrentData) {
				this->rightMotorCurrentSensor->publish_state(rightMotorCurrentData);
			}

			float leftMotorCurrentData = 0.001 * (leftMotorCurrent * 100) / 100;
            if(this->leftMotorCurrentSensor->state != leftMotorCurrentData) {
				this->leftMotorCurrentSensor->publish_state(leftMotorCurrentData);
			}

            float mainBrushCurrentData = 0.001 * (mainBrushCurrent * 100) / 100;
            if(this->mainBrushCurrentSensor->state != mainBrushCurrentData) {
				this->mainBrushCurrentSensor->publish_state(mainBrushCurrentData);
			}

            float sideBrushCurrentData = 0.001 * (sideBrushCurrent * 100) / 100;
            if(this->sideBrushCurrentSensor->state != sideBrushCurrentData) {
				this->sideBrushCurrentSensor->publish_state(sideBrushCurrentData);
			}

			if (virtualWall == 1) {
				this->virtualWallSensor->publish_state(true);
			} else {
				this->virtualWallSensor->publish_state(false);
			}

			if (chargingSources == 0) {
				this->chargingSourcesSensor->publish_state(false);
			} else {
				this->chargingSourcesSensor->publish_state(true);
			}

		}

        // this function can be called from the Roomba yaml file as 
        // static_cast< RoombaComponent*> (id(my_roomba).get_component(0))->send_command("go_forward");
        void send_command(std::string command) {
            on_command(command);
        }

	private:
		uint8_t brcPin;
		uint8_t chargingState;
		int lastWakeupTime = 0;
		bool wasCleaning = false;
		bool wasDocked = false;
		int16_t speed = 0;

		bool lazy650Enabled = false;

		RoombaComponent(uint8_t brcPin, UARTComponent *parent, uint32_t updateInterval, bool lazy650Enabled) : UARTDevice(parent), PollingComponent(updateInterval) {
			this->brcPin = brcPin;
			this->lazy650Enabled = lazy650Enabled;
			this->voltageSensor = new Sensor();
			this->currentSensor = new Sensor();
			this->batteryChargeSensor = new Sensor();
			this->batteryCapacitySensor = new Sensor();
			this->batteryPercentSensor = new Sensor();
			this->batteryTemperatureSensor = new Sensor();
			this->rightMotorCurrentSensor = new Sensor();
			this->leftMotorCurrentSensor = new Sensor();
			this->mainBrushCurrentSensor = new Sensor();
            this->sideBrushCurrentSensor = new Sensor();

			this->chargingSensor = new TextSensor();
			this->activitySensor = new TextSensor();

			this->driveSpeedSensor = new Sensor();
			this->oiModeSensor = new TextSensor();
            this->vacuumSensor = new BinarySensor();
			this->virtualWallSensor = new BinarySensor();
			this->chargingSourcesSensor = new BinarySensor();

		}

		typedef enum {
			Sensors7to26					= 0,  //00
			Sensors7to16					= 1,  //01
			Sensors17to20					= 2,  //02
			Sensors21to26					= 3,  //03
			Sensors27to34					= 4,  //04
			Sensors35to42					= 5,  //05
			Sensors7to42					= 6,  //06
			SensorBumpsAndWheelDrops		= 7,  //07
			SensorWall						= 8,  //08
			SensorCliffLeft					= 9,  //09
			SensorCliffFrontLeft			= 10, //0A
			SensorCliffFrontRight			= 11, //0B
			SensorCliffRight				= 12, //0C
			SensorVirtualWall				= 13, //0D
			SensorOvercurrents				= 14, //0E
			//SensorUnused1					= 15, //0F
			//SensorUnused2					= 16, //10
			SensorIRByte					= 17, //11
			SensorButtons					= 18, //12
			SensorDistance					= 19, //13
			SensorAngle						= 20, //14
			SensorChargingState				= 21, //15
			SensorVoltage					= 22, //16
			SensorCurrent					= 23, //17
			SensorBatteryTemperature		= 24, //18
			SensorBatteryCharge				= 25, //19
			SensorBatteryCapacity			= 26, //1A
			SensorWallSignal				= 27, //1B
			SensoCliffLeftSignal			= 28, //1C
			SensoCliffFrontLeftSignal		= 29, //1D
			SensoCliffFrontRightSignal		= 30, //1E
			SensoCliffRightSignal			= 31, //1F
			SensorUserDigitalInputs			= 32, //20
			SensorUserAnalogInput			= 33, //21
			SensorChargingSourcesAvailable	= 34, //22
			SensorOIMode					= 35, //23
			SensorSongNumber				= 36, //24
			SensorSongPlaying				= 37, //25
			SensorNumberOfStreamPackets		= 38, //26
			SensorVelocity					= 39, //27
			SensorRadius					= 40, //28
			SensorRightVelocity				= 41, //29
			SensorLeftVelocity				= 42, //2A
			SensorLeftMotorCurrent			= 54, //36
			SensorRightMotorCurrent			= 55, //37
            SensorMainBrushCurrent          = 56, //38
            SensorSideBrushCurrent          = 57, //39
		} SensorCode;

		typedef enum {
			ChargeStateNotCharging				= 0,
			ChargeStateReconditioningCharging	= 1,
			ChargeStateFullCharging				= 2,
			ChargeStateTrickleCharging			= 3,
			ChargeStateWaiting					= 4,
			ChargeStateFault					= 5,
		} ChargeState;

        typedef enum {
            ResetCmd        = 7,   //07
			PowerOffCmd		= 133, //85
            StartCmd        = 128, //80
            StopCmd         = 173, //AD
            SafeCmd         = 131, //83
            FullCmd         = 132, //84
            CleanCmd        = 135, //87
			MaxCleanCmd		= 136, //88
            SpotCmd         = 134, //86
            DockCmd         = 143, //8F
            PowerCmd        = 133, //85
            DriveCmd        = 137, //89
            MotorsCmd       = 138, //8A
            LEDAsciiCMD     = 164, //A4
            SongCmd         = 140, //8C
            PlayCmd         = 141, //8D
            SensorsListCmd  = 149, //95
			SetDateCmd		= 168, //A8
        } Commands;      
            
		void brc_wakeup() {
			if (this->lazy650Enabled) {
				ESP_LOGD("roomba", "brc_wakeup");
				pinMode(this->brcPin, OUTPUT);
				digitalWrite(this->brcPin, LOW);
				delay(200);
				pinMode(this->brcPin, OUTPUT);
				delay(200);
				start_oi(); // Start
			} else {
				digitalWrite(this->brcPin, LOW);
				delay(1000);
				digitalWrite(this->brcPin, HIGH);
				delay(100);
			}
		}

		void on_command(std::string command) {
			if (command == "clean") {
				clean();
			} else if (command == "max_clean") {
				maxClean();
			} else if (command == "dock") {
                displayString("DOCK");
				dock();
			} else if (command == "locate") {
				locate();
                displayString("LOC ");
			} else if (command == "clean_spot") {
				spot();
			} else if (command == "stop") {
				stop();
			} else if (command == "wakeup") {
				brc_wakeup();
			} else if (command == "wake_on_dock") {
				wake_on_dock();
			} else if (command == "sleep") {
				sleep();
			} else if (command == "go_max") {
	            ESP_LOGI("roomba", "go max");
				alter_speed(1000);
			} else if (command == "go_forward") {
	            ESP_LOGI("roomba", "go forward");
                this->speed = 200;
                displayString("FWD ");
				drive(this->speed, 0);
			} else if (command == "go_reverse") {
	            ESP_LOGI("roomba", "go reverse");
                this->speed = -200;
                displayString("REV ");
				drive(this->speed, 0);
			} else if (command == "go_faster") {
	            ESP_LOGI("roomba", "go faster");
				alter_speed(100);
			} else if (command == "go_slower") {
	            ESP_LOGI("roomba", "slow it down");
				alter_speed(-100);
			} else if (command == "go_right") {
	            ESP_LOGI("roomba", "go right");
                this->speed = 200;
                displayString("RITE");
				drive(this->speed, -250);
			} else if (command == "go_left") {
	            ESP_LOGI("roomba", "go left");
                this->speed = 200;
                displayString("LEFT");
				drive(this->speed, 250);
			} else if (command == "rotate_right") {
	            ESP_LOGI("roomba", "rotate_right");
                this->speed = 200;
                displayString("ROTR");
				drive(this->speed, 0xFFFF);
			} else if (command == "rotate_left") {
	            ESP_LOGI("roomba", "rotate_left");
                this->speed = 200;
                displayString("ROTL");
				drive(this->speed, 0x0001);
			} else if (command == "stop_driving") {
	            ESP_LOGI("roomba", "STOP");
				this->speed = 0;
                displayString("STOP");
				drive(0,0);
			} else if (command == "drive" || command == "safe") {
	            ESP_LOGI("roomba", "DRIVE (Safe) mode");
				this->speed = 0;
				safeMode();
			} else if (command == "passive") {
	            ESP_LOGI("roomba", "passive mode");
				start_oi();
            } else if (command == "vacuum_on") {
                ESP_LOGI("roomba", "vacuum_on");
                displayString("VAC ");
                vacuum_on();
            } else if (command == "vacuum_off") {
                ESP_LOGI("roomba", "vacuum_off");
                displayString("OFF ");
                vacuum_off();
            } else if (command == "reset") {
                ESP_LOGI("roomba", "reset");
                reset();
			} else if (command == "poweroff") {
                ESP_LOGI("roomba", "poweroff");
                powerOff();
            } else {
                ESP_LOGE("roomba", "unrecognized command %s", command.c_str());
			}            
            //ESP_LOGI("roomba", "ACK %s", command.c_str());
    	}

		void start_oi() {
			write(StartCmd);
		}

        void reset() {
            write(ResetCmd);
        }

		void powerOff() {
			write(PowerOffCmd);
		}

		void locate() {
			uint8_t song[] = {62, 12, 66, 12, 69, 12, 74, 36};
			safeMode();
			delay(500);
			setSong(0, song, sizeof(song));
			playSong(0);
		}
   
		void setSong(uint8_t songNumber, uint8_t data[], uint8_t len){
			write(SongCmd);
			write(songNumber);
			write(len >> 1); // 2 bytes per note
			write_array(data, len);
		}

		void playSong(uint8_t songNumber){
			write(PlayCmd);
			write(songNumber);
		}

        void vacuum_on() {
            write(MotorsCmd);
            write(7); // Main Brush on, Vacuum on, Side brush on
            this->vacuumSensor->publish_state(true);
        }

        void vacuum_off() {
            write(MotorsCmd);
            write(0); // Main Brush off, Vacuum off, Side brush off
            this->vacuumSensor->publish_state(false);
        }

        void displayString(std::string mystring){
            write(LEDAsciiCMD);
            uint8_t asciiValue0 = (int)mystring[0];
            write(asciiValue0);
            uint8_t asciiValue1 = (int)mystring[1];
            write(asciiValue1);
            uint8_t asciiValue2 = (int)mystring[2];
            write(asciiValue2);
            uint8_t asciiValue3 = (int)mystring[3];
            write(asciiValue3);
            delay(50);         
        }

		void wake_on_dock() {
			ESP_LOGD("roomba", "wake_on_dock");
			brc_wakeup();
			// Some black magic from @AndiTheBest to keep the Roomba awake on the dock
			// See https://github.com/johnboiles/esp-roomba-mqtt/issues/3#issuecomment-402096638
			delay(10);
			write(CleanCmd); // Clean
			delay(150);
			write(DockCmd); // Dock
		}

		void alter_speed(int16_t step) {
			int16_t speed = this->speed + step;

			if (speed > 500)
				speed = 500;
			else if (speed < -500)
				speed = -500;

			this->speed = speed;
			this->driveSpeedSensor->publish_state(speed);
			this->drive(this->speed, 0);
		}

		void drive(int16_t velocity, int16_t radius) {
			write(DriveCmd);
			write((velocity & 0xff00) >> 8);
			write(velocity & 0xff);
			write((radius & 0xff00) >> 8);
			write(radius & 0xff);
		}

		void safeMode() {
			write(SafeCmd);
		}

		std::string get_oimode(uint8_t mode) {
			switch(mode) {
				case 0: return "off";
				case 1: return "passive";
				case 2: return "safe";
				case 3: return "full";
				default: return "unknown";
			}
		}

		void clean() {
			write(CleanCmd);
		}

		void maxClean() {
			write(MaxCleanCmd);
		}

		void dock() {
			write(DockCmd);
		}

		void spot() {
			write(SpotCmd);
		}

		void stop() {
			if (wasCleaning) {
				write(CleanCmd);
			}
		}

		void sleep() {
			write(PowerCmd);
		}

		void flush() {
			while (available())
			{
				read();
			}
		}

		bool getSensorsList(uint8_t* packetIDs, uint8_t numPacketIDs, uint8_t* dest, uint8_t len){
			write(SensorsListCmd);
			write(numPacketIDs);
			write_array(packetIDs, numPacketIDs);
			return getData(dest, len);
		}

		bool getData(uint8_t* dest, uint8_t len) {
			while (len-- > 0) {
				unsigned long startTime = millis();
				while (!available()) {
					yield();
					// Look for a timeout
					if (millis() > startTime + ROOMBA_READ_TIMEOUT)
					return false;
				}
				*dest++ = read();
			}
			return true;
		}

		std::string get_activity(uint8_t charging, int16_t current) {
			bool isCharging = charging == ChargeStateReconditioningCharging || charging == ChargeStateFullCharging || charging == ChargeStateTrickleCharging;
			
			if (current > -50)
				return "Docked";
			else if (isCharging)
				return "Charging";
			else if (current < -300)
				return "Cleaning";
			return "Lost";
		}

		inline const char* ToString(uint8_t chargeState) {
			switch (chargeState) {
				case ChargeStateNotCharging:			return "Not Charging";
				case ChargeStateReconditioningCharging:	return "Reconditioning Charging";
				case ChargeStateFullCharging:			return "Full Charging";
				case ChargeStateTrickleCharging:		return "Trickle Charging";
				case ChargeStateWaiting:				return "Waiting";
				case ChargeStateFault:					return "Fault";
				default:								return "Unknown Charging State";
			}
		}
};