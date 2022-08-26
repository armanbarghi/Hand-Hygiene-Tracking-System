from queue import Queue
import paho.mqtt.client as mqtt
import json
import time
import sys
import csv
import os
import threading
import structlog
from enum import Enum, auto


logger = structlog.get_logger(__name__)

class StationController:
    MQTT_SERVER = "127.0.0.1"
    MQTT_PORT = 1883
    MODEL_MAX_SAMPLE = 10

    class State(Enum):
        IDLE = auto()
        ENV_MODEL = auto()

    def __init__(self, mode=State.IDLE) -> None:
        self.state = mode

        # ENV_MODEL state flags
        self.model_sample_counter = 0
        self.current_distance = 1
        #######################
        
        self.client = mqtt.Client('Gateway')

        self.queue = Queue()
        self.data_lst = []

        self.queue_handler_thread = threading.Thread(
            target=self.queue_handler,
            daemon=True
        )
        self.queue_handler_thread.start()
        
        self.initialize_client()
        self.client.loop_forever()

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
                    sys.exit(1)
                attempts += 1
                logger.error("", attempt=attempts, error=error)
            time.sleep(1)

    def queue_handler(self):
        while True:
            if not self.queue.empty():
                message = self.queue.get()
                try:
                    for beacon in message['beacons']:
                        if beacon['ID'] == 'Haylou GT1 XR':
                            self.data_lst.append(int(beacon['RSSI']))
                except Exception as error:
                    logger.error("can not find beacons", error=error)
                    continue

                if self.state == self.State.ENV_MODEL:
                    self.model_env_estimator(message)
            time.sleep(0.1)

    def model_env_estimator(self, message):
        for beacon in message['beacons']:
            if self.model_sample_counter > self.MODEL_MAX_SAMPLE:
                logger.info("sampling done", distance=self.current_distance)
                os._exit(1)
            beacon.update({'station': message['station'], 'distance': self.current_distance})
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

    # region callbacks        
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
    # endregion callbacks