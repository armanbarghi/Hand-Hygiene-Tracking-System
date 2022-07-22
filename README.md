# Hand-Hygiene-Tracking-System
In this project, we are going to use ESP as a BLE beacon and BLE scanner, which beacons are monitored by scanners through Bluetooth Low Energy.
The scanners send the data to the Raspberry Pi through the MQTT protocol, so that the Raspberry Pi collects the information as a database and sends the necessary commands through this protocol.
We use Mosquitto as the broker of MQTT.

Libraries for esp32:
- ESP32
- PubSubClient

Install the ESP32 board in Arduino IDE:

Library: esp32 by Espressif Systems

https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-mac-and-linux-instructions/

To install Mosquitto Broker:

`sudo apt install -y mosquitto mosquitto-clients`

To make Mosquitto broker automatically start with the boot:

`sudo systemctl enable mosquitto.service`

Test the installation:

`mosquitto -v`

Stop moquitto broker:

`sudo systemctl stop mosquitto.service`
