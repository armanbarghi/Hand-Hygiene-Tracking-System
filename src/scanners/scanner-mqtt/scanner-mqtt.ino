#include <string>

#define MAX_PACKET_SIZE 2048

// Wifi and MQTT
#include <WiFi.h>
#include <PubSubClient.h>

// Bluetooth LE
#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"

#include "configs.h"

WiFiClient espClient;
PubSubClient client(espClient);

// LED Pin
const int ledPin = 2;

int beacon_rssi;
char beacon_name[20];
char rssi_string[3];

typedef struct {
  char name[20];
  int id;
  int rssi;
} Beacon;

Beacon buffer[max_buffer_len];
uint8_t buffer_index = 0;
uint8_t message_char_buffer[MAX_PACKET_SIZE];

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice *advertisedDevice)
    {
      extern uint8_t buffer_index;
      extern Beacon buffer[];
      if (buffer_index >= max_buffer_len) {
        return;
      }
      if (advertisedDevice->haveRSSI()) {
        if (strcmp(advertisedDevice->getName().c_str(), "Haylou GT1 XR") == 0) {
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

void setup()
{
  Serial.begin(115200);
  BLEDevice::init("");
  connectWifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(MyMqttDeviceCallback);
  checkMQTT();
  pinMode(ledPin, OUTPUT);
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
  BLEScan* pBLEScan = BLEDevice::getScan();
  MyAdvertisedDeviceCallbacks cb;
  pBLEScan->setAdvertisedDeviceCallbacks(&cb);
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->start(beacon_scan_timeout);
  Serial.print("Devices found: ");
  for (uint8_t i = 0; i < buffer_index; i++) {
    Serial.print(buffer[i].name);
    Serial.print(" : ");
    Serial.println(buffer[i].rssi);
  }
  pBLEScan->stop();
}

void sendFromBuffer() {
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

  payload.getBytes(message_char_buffer, payload.length() + 1);
  client.publish("esp32/scan", message_char_buffer, payload.length(), false);
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
