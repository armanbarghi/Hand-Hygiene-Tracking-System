#include <string>

// Wifi and MQTT
#include <WiFi.h>
#include <PubSubClient.h>

// Bluetooth LE
#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"

const char* ssid = "ArmPoc";
const char* password = "123456789";
const char* mqtt_server = "192.168.1.6";
const int mqtt_port = 1883;
long lastMsg = 0;

WiFiClient espClient;
PubSubClient client(espClient);

const int scanTime = 1; //In seconds
BLEScan *pBLEScan;

// LED Pin
const int ledPin = 2;

char beacon_name[20];
int beacon_rssi;
char rssi_string[3];

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice *advertisedDevice)
    {
      if (advertisedDevice->haveName())
      {
        Serial.print("Device name: ");
        Serial.println(advertisedDevice->getName().c_str());
        Serial.print("RSSI: ");
        Serial.println(advertisedDevice->getRSSI());
        Serial.println("");
        beacon_rssi = advertisedDevice->getRSSI();
        strcpy(beacon_name, advertisedDevice->getName().c_str());
        itoa(beacon_rssi, rssi_string, 10);
      }
    }
};

void MyMqttDeviceCallback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if (messageTemp == "on") {
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if (messageTemp == "off") {
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(MyMqttDeviceCallback);

  BLEDevice::init("ESP32_BLEScanner");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); // less or equal setInterval value
  
  pinMode(ledPin, OUTPUT);
}

void loop()
{
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  Serial.println("Scanning...");
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  Serial.print("Devices found: ");
  Serial.println(foundDevices.getCount());
  Serial.println("-----------------------------------------------\r\n");
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  
  client.publish("esp32/ble_name", beacon_name);
  client.publish("esp32/ble_rssi", rssi_string);
}
