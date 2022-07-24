# Hand-Hygiene-Tracking-System
In this project, we are going to use ESP as a BLE beacon and BLE scanner, which beacons are monitored by scanners through Bluetooth Low Energy.
The scanners send the data to the Raspberry Pi through the MQTT protocol, so that the Raspberry Pi collects the information as a database and sends the necessary commands through this protocol.
We use Mosquitto as the broker of MQTT.

Libraries for esp32:
1. esp32 by Espressif Systems
- Installation guide [here](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-mac-and-linux-instructions/)
2. NimBLE-Arduino
3. PubSubClient

To install Mosquitto Broker:

`sudo apt install -y mosquitto mosquitto-clients`

To make Mosquitto broker automatically start with the boot:

`sudo systemctl enable mosquitto.service`

Test the installation:

`mosquitto -v`

Stop moquitto broker:

`sudo systemctl stop mosquitto.service`

Sketch uses 552149 bytes (42%) of program storage space. Maximum is 1310720 bytes.

With just use of mqtt code, the sketch uses 52% of program storage space and using ble scanner example from the original library, will uses 78% of program storage space. So, we can't use ble and wifi at the same time. As an alternative solution we use another library from [here](https://github.com/h2zero/NimBLE-Arduino).