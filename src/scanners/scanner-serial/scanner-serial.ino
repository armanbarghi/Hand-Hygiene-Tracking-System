#include <string>

#define MQTT_MAX_PACKET_SIZE 2048

// Bluetooth LE
#include "BLEDevice.h"
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "configs.h"

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
uint8_t message_char_buffer[MQTT_MAX_PACKET_SIZE];

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  public:
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      extern uint8_t buffer_index;
      extern Beacon buffer[];
      if (buffer_index >= max_buffer_len) {
        return;
      }
      if (advertisedDevice.haveRSSI()) {
        if (strcmp(advertisedDevice.getName().c_str(), "Haylou GT1 XR") == 0) {
          itoa(advertisedDevice.getRSSI(), rssi_string, 10);
          buffer[buffer_index].rssi = advertisedDevice.getRSSI();  
          strcpy(buffer[buffer_index].name, advertisedDevice.getName().c_str());
          buffer_index++;
        }
      }
    }
};

void setup()
{
  Serial.begin(115200);
  BLEDevice::init("");
  pinMode(ledPin, OUTPUT);
}

void scanBeacons() {
  // for this error:
  // "Guru meditation error: Core 1 panic'ed (LoadProhinited)"
  // we need to define pBLEScan pointer each time
  BLEScan* pBLEScan = BLEDevice::getScan();
  MyAdvertisedDeviceCallbacks cb;
  pBLEScan->setAdvertisedDeviceCallbacks(&cb);
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->start(beacon_scan_timeout, false);
  pBLEScan->stop();
  Serial.println("");
  Serial.print("buffer index: ");
  Serial.println(buffer_index);
}

void sendFromBuffer() {
  bool result;
  String payload = "HEAD{\"beacons\":[";
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
  payload += "\"}FOOT";

  Serial.print(payload);
}

void loop()
{
//  delay(1000);
  scanBeacons();
//  delay(1000);
  if (buffer_index) {
    sendFromBuffer();
    //Start over the scan loop
    buffer_index = 0;
  }
}
