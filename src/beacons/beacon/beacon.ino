#include "sys/time.h"

#include <Arduino.h>

#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEBeacon.h"
#include "BLEAdvertising.h"

#include "esp_sleep.h"

#define GPIO_DEEP_SLEEP_DURATION 0.5     // sleep x seconds and then wake up
RTC_DATA_ATTR static time_t last;    // remember last boot in RTC Memory
RTC_DATA_ATTR static uint32_t bootcount; // remember number of boots in RTC Memory
time_t lastTenth;
struct timeval nowTimeStruct;

BLEServer *pServer = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID  "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
 
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
 
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);
 
        Serial.println();
        Serial.println("*********");
      }
    }
};

void setup()
{
  Serial.begin(115200);

  gettimeofday(&nowTimeStruct, NULL);
  Serial.printf("start ESP32 %d\n", bootcount++);
  Serial.printf("deep sleep (%lds since last reset, %lds since last boot)\n", nowTimeStruct.tv_sec, nowTimeStruct.tv_sec - last);
  last = nowTimeStruct.tv_sec;
  lastTenth = nowTimeStruct.tv_sec * 10; // Time since last reset as 0.1 second resolution counter

  // Create the BLE Device
  BLEDevice::init("Haylou GT1 XR");
  // BLEDevice::setPower(ESP_PWR_LVL_P7, ESP_BLE_PWR_TYPE_ADV);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                            CHARACTERISTIC_UUID_RX,
                                            BLECharacteristic::PROPERTY_WRITE
                                        );
 
  pRxCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the service
  pService->start();
  
  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

//  Serial.printf("enter deep sleep for 10s\n");
//  esp_deep_sleep(1000000LL * GPIO_DEEP_SLEEP_DURATION);
//  Serial.printf("in deep sleep\n");
}

void loop()
{
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        Serial.println("Connected!");
        oldDeviceConnected = deviceConnected;
    }
}
