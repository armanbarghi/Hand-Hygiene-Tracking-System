from .kalman import KalmanFilter2D, KalmanFilter1D
from .moving_average import StreamingMovingAverage
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
    MODEL_MAX_SAMPLE = 200
    PATH_LOSS_EXPONENT = 2.4852
    INITIAL_DISTANCE = 0.5
    INITIAL_RSSI = -34

    class State(Enum):
        IDLE = auto()
        ENV_MODEL = auto()
        MONITOR = auto()
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
        self.current_distance = 0.5
        ########################

        # WORKING state fields
        self.beacons = []
        self.stations = {
            '1': (0.25,0),
            '2': (3.5,0),
            '3': (6.5,0.25),
            '4': (6.25,5.5)
        }
        ######################

        self.queue = Queue()
        self.data_lst = []

        self.initial_filters()

    # region mqtt
    def start_mqtt(self):
        self.client = mqtt.Client('Gateway')
        self.initialize_client()
        self.client.loop_start()

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
            logger.debug('decoded data', topic=msg.topic, decoded=decoded)
            self.queue.put(json.loads(decoded))
        except Exception as error:
            logger.error("decoding message failed", error=error)
    # endregion mqtt

    # region serial
    def start_serial(self):
        self.rx_buffer = b''
        self.ser_ready = False
        self.ser = None
        self.initialize_serial()
        serial_recieve_thread = threading.Thread(
            target=self._serial_recieve,
            daemon=True
        )
        serial_recieve_thread.start()

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
            logger.debug("serial read", decoded=decoded)
            self.queue.put(json.loads(decoded))   
        except Exception as error:
            logger.error("decoding message failed", error=error)
    # endregion serial

    def initial_filters(self):
        self.KF = KalmanFilter2D(
            dt=0.1, 
            u_x=1, 
            u_y=1, 
            std_acc=1, 
            x_std_meas=0.1, 
            y_std_meas=0.1
        )

        self.KF1D = KalmanFilter1D(
            u_x=1, 
            std_acc=0.5, 
            std_meas=1
        )

        self.SMA = StreamingMovingAverage(window_size=10)

    def start(self):
        while True:
            if not self.queue.empty():
                message = self.queue.get()
                for beacon in message['beacons']:
                    if self.state == self.State.ENV_MODEL:
                        self.model_env_handler(beacon, message['station'])
                    elif self.state == self.State.WORKING:
                        self.working_state_handler(beacon['ID'], int(beacon['RSSI']), message['station'])
                    elif self.state == self.State.MONITOR:
                        self.monitor_handler(int(beacon['RSSI']))
            time.sleep(0.01)

    def model_env_handler(self, beacon, station):
        if self.model_sample_counter >= self.MODEL_MAX_SAMPLE:
            logger.info("sampling done", distance=self.current_distance)
            os._exit(1)
        beacon.update({'station': station, 'distance': self.current_distance})
        self.append_in_file("env_model.csv", beacon)
        self.model_sample_counter += 1

    def working_state_handler(self, beacon_id, beacon_rssi, station):
        if self.find_beacon_by_id(beacon_id) is None:
            self.add_new_beacon(beacon_id)
        beacon = self.find_beacon_by_id(beacon_id)
        distance = self.convert_rssi_to_distance(self.SMA.process(beacon_rssi))
        beacon['stations'][station] = distance
        if self.is_ready_to_trilateration(beacon):
            prediction = self.KF.predict()
            measured = self.calc_trilateration(beacon)
            coordinates = self.KF.update(measured)
            beacon['coordinates'] = coordinates
            self.update_beacon_status(beacon)
            self.append_in_file("kalman.csv", {'Measured':measured, 'Prediction':prediction, 'Coordinated':coordinates})
        logger.info("beacon updated", beacons=self.beacons)
        # self.append_in_file("stations.csv", {'ID':beacon_id, 'RSSI':beacon_rssi, 'station':station})

    def monitor_handler(self, beacon_rssi):
        if self.model_sample_counter >= self.MODEL_MAX_SAMPLE:
            logger.info("monitoring done")
            os._exit(1)
        self.KF1D.predict()
        kf_rssi = self.KF1D.update(beacon_rssi)
        sma_rssi = self.SMA.process(beacon_rssi)
        self.append_in_file("monitor.csv", {'RSSI': beacon_rssi, 'KF': kf_rssi, 'SMA': sma_rssi})
        self.model_sample_counter += 1

    def update_beacon_status(self, beacon):
        pass

    def convert_rssi_to_distance(self, rssi):
        return self.INITIAL_DISTANCE*10**( (self.INITIAL_RSSI-rssi) / 10 / self.PATH_LOSS_EXPONENT )

    def is_ready_to_trilateration(self, beacon):
        return not None in list(beacon['stations'].values())

    def calc_trilateration(self, beacon):
        tri_distance = sorted(beacon['stations'].items(), key=lambda x:x[1])[:3]
        xa, ya = self.stations[tri_distance[0][0]]
        xb, yb = self.stations[tri_distance[1][0]]
        xc, yc = self.stations[tri_distance[2][0]]
        ra = tri_distance[0][1]
        rb = tri_distance[1][1]
        rc = tri_distance[2][1] 
        A1 = 2*(xb-xa)
        A2 = 2*(xc-xa)
        A3 = 2*(xc-xb)
        B1 = 2*(yb-ya)
        B2 = 2*(yc-ya)
        B3 = 2*(yc-yb)
        C1 = ra**2 - rb**2 + xb**2 - xa**2 + yb**2 - ya**2
        C2 = ra**2 - rc**2 + xc**2 - xa**2 + yc**2 - ya**2
        C3 = rb**2 - rc**2 + xc**2 - xb**2 + yc**2 - yb**2
        S = np.linalg.inv([
            [A1**2 + A2**2 + A3**2 , A1*B1 + A2*B2 + A3*B3],
            [A1*B1 + A2*B2 + A3*B3 , B1**2 + B2**2 + B3**2]
        ])
        T = [ [A1*C1 + A2*C2 + A3*C3] , [B1*C1 + B2*C2 + B3*C3] ]
        return tuple(np.dot(S, T).reshape(1,-1)[0])

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

    def append_in_file(self, file_name: str, content: dict):
        try:
            file_exists = os.path.isfile(file_name)
            with open(file_name, "a") as file:
                writer = csv.DictWriter(file, fieldnames=content.keys())
                if not file_exists:
                    writer.writeheader()
                writer.writerow(content)
                file.close()
        except Exception as error:
            logger.error("can not write in file", error=error)

    # region sending message to beacon
    def send_message_to_beacon(self, beacon, message):
        station = self.find_nearest_station_to_beacon(beacon)
        self.publish('esp32/'+station, message)

    def find_nearest_station_to_beacon(self, beacon):
        new_list = []
        for st in list(beacon['stations'].items()):
            if not st[1] is None:
                new_list.append(st)
        return sorted(new_list, key=lambda x:x[1])[0][0]
    
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