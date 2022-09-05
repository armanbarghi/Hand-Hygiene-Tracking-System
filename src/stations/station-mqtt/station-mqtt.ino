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


// The remote service we wish to connect to.
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");

boolean do_beacon_connect = false;
boolean find_beacon = false;
boolean beacon_connected = false;
boolean do_scan = true;
BLERemoteCharacteristic* pRemoteCharacteristic;
BLEAdvertisedDevice* beacon = NULL;
BLEClient* pClient = NULL;
String command;
char beacon_to_connect_name[20];

bool isKnownBeacon(const char* name) {
  if(strcmp(name, "B01") == 0 || strcmp(name, "B02") == 0 || strcmp(name, "B03") == 0 || strcmp(name, "B04") == 0 || strcmp(name, "B05") == 0) {
    return true;
  }
  return false;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice *advertisedDevice)
    {
      extern uint8_t buffer_index;
      extern Beacon buffer[];
      if (buffer_index >= max_buffer_len) {
        return;
      }
      if (advertisedDevice->haveName()) {
        if (advertisedDevice->haveRSSI()) {
          if (isKnownBeacon(advertisedDevice->getName().c_str())) {
            itoa(advertisedDevice->getRSSI(), rssi_string, 10);
            buffer[buffer_index].rssi = advertisedDevice->getRSSI();  
            strcpy(buffer[buffer_index].name, advertisedDevice->getName().c_str());
            buffer_index++;
            BLEDevice::getScan()->stop(); // Remove this for multi scanning
          }
        }
        if (find_beacon) {
          if (strcmp(advertisedDevice->getName().c_str(), beacon_to_connect_name) == 0) {
            Serial.println("Found beacon to connect");
            BLEDevice::getScan()->stop();
            Serial.println("Scanning stopped");
            beacon = advertisedDevice;
            do_beacon_connect = true;
            find_beacon = false;
          }
        }
      }
    }
};

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    beacon_connected = false;
    Serial.println("disconnected from beacon");
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
  char temp_topic[10] = "esp32/";
  strcat(temp_topic, station_id);

  if (strcmp(topic, "esp32/led") == 0) {
    if (messageTemp == "on") {
      digitalWrite(ledPin, HIGH);
    }
    else if (messageTemp == "off") {
      digitalWrite(ledPin, LOW);
    }
  }
  else if (strcmp(topic, temp_topic) == 0) {
    strcpy(beacon_to_connect_name, "B01");
    find_beacon = true;
    
    command = messageTemp;
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
      Serial.println("esp32/led subscribed!");
      char topic[10] = "esp32/";
      strcat(topic, station_id);
      client.subscribe(topic);
      Serial.print(topic);
      Serial.println(" subscribed!");
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

bool connectToBeacon() {
  Serial.print("Forming a connection to ");
  Serial.println(beacon_to_connect_name);
  
  pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remove BLE Server.
  pClient->connect(beacon);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if(pRemoteCharacteristic->canRead()) {
    std::string value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());
  }
  
  beacon_connected = true;
  return true;
}

void checkBLE() {
  if (do_beacon_connect) {
    if (connectToBeacon()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    do_beacon_connect = false;
  }
}

void setup()
{
  Serial.begin(115200);
  BLEDevice::init("");
  BLEDevice::setPower(ESP_PWR_LVL_P7, ESP_BLE_PWR_TYPE_SCAN);
  connectWifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(MyMqttDeviceCallback);
  checkMQTT();
  pinMode(ledPin, OUTPUT);
}

void loop()
{
  if (find_beacon)
  {
    Serial.print("looking for beacon:");
    Serial.println(beacon_to_connect_name);
  }
  delay(100);
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

  checkBLE();
  if (beacon_connected) {
    pRemoteCharacteristic->writeValue(command.c_str(), command.length());
    pClient->disconnect();
  }
}
