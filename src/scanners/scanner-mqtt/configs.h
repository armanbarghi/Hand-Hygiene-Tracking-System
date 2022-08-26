#ifndef __CONFIGS_H__
#define __CONFIGS_H__

// WiFi username and password
const char* ssid = "ArmPoc";
const char* password = "a135246789";

// MATT server and port
const char* mqtt_server = "192.168.247.77";
const int mqtt_port = 1883;

const char* station_id = "1";

const int beacon_scan_timeout = 1; //In seconds

const int max_buffer_len = 50;

#endif
