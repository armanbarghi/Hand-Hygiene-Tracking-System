# Hand-Hygiene-Tracking-System

About
-----

In this project, we are going to use ESP as a BLE beacon and BLE stations, which beacons are monitored by 
stations through Bluetooth Low Energy.
The stations send the data to the server through the MQTT protocol, so that the server collects the information as a database and sends the necessary commands through this protocol.
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
With just the MQTT code, the sketch uses 52% of program storage space, and using ble scanner example from the original library will use 78% of program storage space. So, we can't use ble and wifi at the same time. As an alternative solution we use another library from [here](https://github.com/h2zero/NimBLE-Arduino) that only uses 42% of program storage space.

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

Step 1. Create a virtual environment.
```sh
python3 -m venv myvenv
```
Step 2. Activate the virtual environment (you can activate venv just by reopening the terminal instead of following commands).
```sh
# For Linux
source myvenv/bin/activate
# For Windows
./myvenv/Scripts/activate
```
Step 3. After activating your virtual environment, install the ipykernel for Jupyter Notebook.
```sh
pip3 install ipykernel
```
Step 4. (Optional) You can create a new kernel by the following command or just skip this step and select 'myvenv' kernel in the step 7.
```sh
python3 -m ipykernel install --user --name=myproject
```
Step 5. Open a jupyter-notebook in VS Code by holding 'Ctrl+Shift+P' and selecting 'Create: New Jupyter Notebook'.
Step 6. Select your kernel
- The one you created in Step 4.
- Or just use this one: Python Environments ->  myvenv (Python)
Step 7. Then, install all the required libraries specified in the 'requirements.txt' file.
```sh
pip3 install -r requirements.txt
```
