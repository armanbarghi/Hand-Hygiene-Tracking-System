#include <string>

// Wifi and MQTT
#include <WiFi.h>
#define MQTT_MAX_PACKET_SIZE 2048
#include <PubSubClient.h>

// Bluetooth LE
#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"

#include "configs.h"

WiFiClient espClient;
PubSubClient client(espClient);

BLEScan *pBLEScan;

// LED Pin
const int ledPin = 2;

long lastMsg = 0;
int beacon_rssi;
char beacon_name[20];
char rssi_string[3];

typedef struct {
  char name[20];
  int id;
  int rssi;
} Beacon;

Beacon buffer[10];
uint8_t buffer_index = 0;
uint8_t message_char_buffer[MQTT_MAX_PACKET_SIZE];

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice *advertisedDevice)
    {
      if (advertisedDevice->haveRSSI()) {
        if (strcmp(advertisedDevice->getName(), "Haylou GT1 XR") == 0) {
          itoa(advertisedDevice->getRSSI(), rssi_string, 10);
          buffer[buffer_index].rssi = advertisedDevice->getRSSI();  
          strcpy(buffer[buffer_index].name, advertisedDevice->getName().c_str());
          buffer_index++;
        }
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

  if (String(topic) == "esp32/led") {
    if (messageTemp == "on") {
      digitalWrite(ledPin, HIGH);
    }
    else if (messageTemp == "off") {
      digitalWrite(ledPin, LOW);
    }
  }
}

void connectWifi() {
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

void checkWifi() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }
}

void checkMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(station_id)) {
      Serial.println("connected");
      client.subscribe("esp32/led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

void scanBeacons() {
  // for this error:
  // "Guru meditation error: Core 1 panic'ed (LoadProhinited)"
  // we need to define pBLEScan pointer each time
  Serial.println("Beacon scanning...");
  pBLEScan = BLEDevice::getScan(); //create 1
  MyAdvertisedDeviceCallbacks cb;
  pBLEScan->setAdvertisedDeviceCallbacks(&cb);
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  BLEScanResults foundDevices = pBLEScan->start(beacon_scan_timeout);
  Serial.print("Devices found: ");
  Serial.println(foundDevices.getCount());
  Serial.println("-----------------------------------------------\r\n");
//  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  pBLEScan->stop();
}

void setup()
{
  Serial.begin(115200);
  BLEDevice::init("");
  connectWifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(MyMqttDeviceCallback);
  pinMode(ledPin, OUTPUT);
}

void sendFromBuffer() {
  bool result;
  String payload = "{\"beacons\":[";
  for (uint8_t i = 0; i < buffer_index; i++) {
    payload += "{\"ID\":\"";
    payload += String(buffer[i].name);  // FIXME: change it to id
    payload += "\",\"RSSI\":\"";
    payload += String(buffer[i].rssi);
    payload += "\"}";
    if (i < buffer_index - 1) {
      payload += ',';
    }
  }
  // SenML ends. Add this stations ID
  payload += "],\"station\":\"";
  payload += String(station_id);
  payload += "\"}";

  // Print and publish payload
//  Serial.print("Payload length: ");
//  Serial.println(payload.length());
//  Serial.println(payload);

  payload.getBytes(message_char_buffer, payload.length() + 1);
  result = client.publish("esp32/scan", message_char_buffer, payload.length(), false);
//  Serial.print("PUB Result: ");
//  Serial.println(result);
}

void loop()
{
  scanBeacons();
  checkWifi();
  checkMQTT();
  client.loop();
  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    if (buffer_index) {
      sendFromBuffer();
      //Start over the scan loop
      buffer_index = 0;
    }
  }
}