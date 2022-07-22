# Hand-Hygiene-Tracking-System
In this project, we are using ESP32 as a beacon and as a client.
And connect the client to RPi by MQTT protocol to communicate with each other.
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

To test the installation by running the following command:

`mosquitto -v`

We can stop the moquitto broker by the following command:

`sudo systemctl stop mosquitto.service`
