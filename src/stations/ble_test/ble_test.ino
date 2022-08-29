/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

/** NimBLE differences highlighted in comment blocks **/

/*******original********
  #include <BLEDevice.h>
  #include <BLEUtils.h>
  #include <BLEScan.h>
  #include <BLEAdvertisedDevice.h>
  #include "BLEEddystoneURL.h"
  #include "BLEEddystoneTLM.h"
  #include "BLEBeacon.h"
***********************/

#include <Arduino.h>

#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEEddystoneURL.h"
#include "NimBLEEddystoneTLM.h"
#include "NimBLEBeacon.h"

int scanTime = 1; //In seconds
BLEScan *pBLEScan;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /*** Only a reference to the advertised device is passed now
      void onResult(BLEAdvertisedDevice advertisedDevice) { **/
    void onResult(BLEAdvertisedDevice *advertisedDevice)
    {
      if (advertisedDevice->haveRSSI() && advertisedDevice->haveName())
      {
        if (advertisedDevice->getName() == "ESp32 SHIT")
        {
//          Serial.print("Device name: ");
//          Serial.println(advertisedDevice->getName().c_str());
//          Serial.print("\tRSSI: ");
          Serial.println(advertisedDevice->getRSSI());
        }
      }
    }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Scanning...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(10);
  pBLEScan->setWindow(9); // less or equal setInterval value
}

void loop()
{
//  Serial.println("Scanning...");
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
//  Serial.print("Devices found: ");
//  Serial.println(foundDevices.getCount());
//  Serial.println("Scan done!");
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  delay(100);
}
