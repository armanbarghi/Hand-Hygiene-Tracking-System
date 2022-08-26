# Hand-Hygiene-Tracking-System

About
-----

In this project, we are going to use ESP as a BLE beacon and BLE scanner, which beacons are monitored by scanners through Bluetooth Low Energy.
The scanners send the data to the Raspberry Pi through the MQTT protocol, so that the Raspberry Pi collects the information as a database and sends the necessary commands through this protocol.
We use Mosquitto as the broker of MQTT.

Libraries for esp32
-------------------

1. esp32 by Espressif Systems [(Installation guide)](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-mac-and-linux-instructions/)
2. NimBLE-Arduino
3. PubSubClient

Documentation
-------------

To install Mosquitto Broker:
```sh
sudo apt install -y mosquitto mosquitto-clients
```
To make Mosquitto broker automatically start with the boot:
```sh
sudo systemctl enable mosquitto.service
```
Test the installation:
```sh
mosquitto -v
```
Stop moquitto broker:
```sh
sudo systemctl stop mosquitto.service
```
With just use of mqtt code, the sketch uses 52% of program storage space and using ble scanner example from the original library, will uses 78% of program storage space. So, we can't use ble and wifi at the same time. As an alternative solution we use another library from [here](https://github.com/h2zero/NimBLE-Arduino) that only uses 42% of program storage space.

If the connection to the MQTT server is refused, check the configuration file:
```sh
cat /etc/mosquitto/conf.d/standard.conf
```
It has to be something like this:
```sh
listener 1883
protocol mqtt
allow_anonymous true
```
then restart the mosquitto.service:
```sh
sudo systemctl restart mosquitto.service
```

Installation
------------

First of all we need to create a virtual environment:
```sh
python3 -m venv venv
```
Then we will install all the requierments libraries
```sh
pip3 install -r requierments.txt
```

