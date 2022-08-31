from queue import Queue
import paho.mqtt.client as mqtt
import serial.tools.list_ports as port_list
from serial import SerialException
import serial
import json
import time
import sys
import csv
import os
import threading
import structlog
from enum import Enum, auto


logger = structlog.get_logger(__name__)

class ServerController:
    MQTT_SERVER = "127.0.0.1"
    MQTT_PORT = 1883
    MAX_SERIAL_BUFFER_LEN = 100
    HEAD = b'HEAD'
    FOOT = b'FOOT'
    MODEL_MAX_SAMPLE = 10

    class State(Enum):
        IDLE = auto()
        ENV_MODEL = auto()
        WORKING = auto()
    
    class Beacon(Enum):
        IDLE = auto()
        RED = auto()
        GREEN = auto()
        ORANGE = auto()

    def __init__(self, mode=State.IDLE) -> None:
        self.state = mode
        logger.info("server created", state=self.state)

        # ENV_MODEL state fields
        self.model_sample_counter = 0
        self.current_distance = 2
        ########################

        # WORKING state fields
        self.beacons = []
        ######################

        self.queue = Queue()
        self.data_lst = []

        ### MQTT section
        self.client = mqtt.Client('Gateway')
        self.initialize_client()
        self.client.loop_start()

        ### Serial section
        self.rx_buffer = b''
        self.ser_ready = False
        self.ser = None
        self.initialize_serial()
        serial_recieve_thread = threading.Thread(
            target=self._serial_recieve,
            daemon=True
        )
        serial_recieve_thread.start()

        self.queue_handler()

    # region mqtt
    def initialize_client(self):
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_publish = self._on_publish
        self.client.message_callback_add('esp32/scan', self._on_message)
        self.connect_to_broker()
        self.client.subscribe('esp32/#')

    def connect_to_broker(self):
        attempts = 0
        logger.info("searching for the broker")
        while not self.client.is_connected():
            try:
                self.client.connect(self.MQTT_SERVER, self.MQTT_PORT)
                return
            except Exception as error:
                if attempts >= 3:
                    logger.error("mqtt connection failed")
                    return
                attempts += 1
                logger.error("", attempt=attempts, error=error)
            time.sleep(1)

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info('MQTT connected', rc=rc)
            client.connected_flag=True #flag to indicate success
        else:
            logger.error('MQTT can not connect', rc=rc)
            client.bad_connection_flag=True
            sys.exit(1)

    def _on_disconnect(self, client, userdata, rc):
        logger.error('MQTT disconnected', rc=rc)

    def _on_publish(self, client, userdata, mid):
        ...

    def _on_message(self, client, userdata, msg):
        try:
            decoded = msg.payload.decode('utf-8')
            logger.info('decoded data', topic=msg.topic, decoded=decoded)
            self.queue.put(json.loads(decoded))
        except Exception as error:
            logger.error("decoding message failed", error=error)
    # endregion mqtt

    # region serial
    def initialize_serial(self):
        attempts = 0
        try:
            while attempts < 3:
                attempts += 1
                ports = [tuple(p) for p in list(port_list.comports())]
                logger.info("searching for serial ports", ports=ports, attempt=attempts)
                if ports:
                    port_name = ports[0][0]
                    if "ttyUSB" in port_name:
                        self.ser = serial.Serial(
                            port=port_name,
                            baudrate=115200,
                            timeout=None
                        )
                        if self.ser.is_open:
                            self.ser.close()
                        self.ser.open()
                        self.ser_ready = True
                        logger.info("serial connected", port=port_name)
                        return
                time.sleep(1)
            logger.error("serial connection failed")
        except SerialException as error:
            logger.error("unable to open serial", error=error)
            sys.exit(1)
    
    def _serial_recieve(self):
        while self.ser_ready:
            in_waiting = self.ser.in_waiting
            if in_waiting:
                self.rx_buffer += self.ser.read_all()
                self.serial_decode()
                if len(self.rx_buffer) > self.MAX_SERIAL_BUFFER_LEN:
                    self.rx_buffer = b''
            time.sleep(0.01)
    
    def serial_decode(self):
        if not self.HEAD in self.rx_buffer:
            return
        self.rx_buffer = self.rx_buffer[self.rx_buffer.find(self.HEAD):]
        if not self.FOOT in self.rx_buffer:
            return
        footer_index = self.rx_buffer.find(self.FOOT)
        serial_data = self.rx_buffer[len(self.HEAD):footer_index]
        self.rx_buffer = self.rx_buffer[footer_index+len(self.FOOT):]
        try:
            decoded = serial_data.decode('utf-8')
            logger.info("serial read", decoded=decoded)
            self.queue.put(json.loads(decoded))   
        except Exception as error:
            logger.error("decoding message failed", error=error)
    # endregion serial

    def queue_handler(self):
        while True:
            if not self.queue.empty():
                message = self.queue.get()
                for beacon in message['beacons']:
                    if self.state == self.State.ENV_MODEL:
                        self.model_env_estimator(beacon, message['station'])
                    if self.state == self.State.WORKING:
                        self.working_state_handler(beacon['ID'], int(beacon['RSSI']), message['station'])
            time.sleep(0.01)

    def model_env_estimator(self, beacon, station):
        if self.model_sample_counter > self.MODEL_MAX_SAMPLE:
            logger.info("sampling done", distance=self.current_distance)
            os._exit(1)
        beacon.update({'station': station, 'distance': self.current_distance})
        self.append_in_file("env_model.csv", beacon)
        self.model_sample_counter += 1

    def append_in_file(self, file_name, content):
        try:
            file_exists = os.path.isfile(file_name)
            with open(file_name, "a") as file:
                writer = csv.DictWriter(file, fieldnames=['ID','RSSI','station', 'distance'])
                if not file_exists:
                    writer.writeheader()
                writer.writerow(content)
                file.close()
        except Exception as error:
            logger.error("can not write in file", error=error)

    def working_state_handler(self, beacon_id, beacon_rssi, station):
        # if beacon_id == 'Haylou GT1 XR':
        #     self.data_lst.append(beacon_rssi)
        if self.find_beacon_by_id(beacon_id) is None:
            self.add_new_beacon(beacon_id)
        beacon = self.find_beacon_by_id(beacon_id)
        beacon['stations'][station] = beacon_rssi
        logger.info("beacon updated", beacons=self.beacons)
    
    def check_stations(self, beacon):
        ...

    def find_beacon_by_id(self, beacon_id):
        for beacon in self.beacons:
            if beacon['id'] == beacon_id:
                return beacon
        return None

    def add_new_beacon(self, id):
        beacon = {
            'id' : id,
            'status': self.Beacon.IDLE, 
            'stations': {'1':None, '2':None, '3':None, '4':None},
            'coordinates': (None,None) 
        }
        self.beacons.append(beacon)

    # region sending message to beacon
    def send_message_to_beacon(self, beacon, message):
        station = self.find_nearest_station_to_beacon(beacon)
        self.publish('esp32/'+str(station), message)

    def find_nearest_station_to_beacon(self, beacon):
        # return min(beacon['stations'])
        return 1
    
    def publish(self, topic, message):
        payload = message.encode('utf-8')
        info = self.client.publish(
            topic=topic,
            payload=payload,
            qos=0,
        )
        # Because published() is not synchronous,
        # it returns false while he is not aware of delivery that's why calling wait_for_publish() is mandatory.
        info.wait_for_publish()
        logger.info("published", payload=payload, info=info)
    # endregion sending message to beacon