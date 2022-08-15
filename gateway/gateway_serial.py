from base64 import decode
from distutils.filelist import translate_pattern
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import serial.tools.list_ports as port_list
import json
import time
import serial
import threading
import structlog


logger = structlog.get_logger(__name__)
data_lst = []
fig, ax = plt.subplots()
rx_buffer = b''

def animate(i, data_lst):
    ax.clear()
    plt.grid()
    ax.plot(data_lst)
    data_lst = data_lst[-100:]
    ax.set_ylim([-100,0])

def serial_recieve():
    global rx_buffer
    while ser_ready:
        in_waiting = ser.in_waiting
        if in_waiting:
            rx_buffer += ser.read_all()
            decode_data()
        time.sleep(0.01)

def decode_data():
    global data_lst
    global rx_buffer
    HEAD = b'HEAD'
    FOOT = b'FOOT'
    if not HEAD in rx_buffer:
        return
    rx_buffer = rx_buffer[rx_buffer.find(HEAD):]
    if not FOOT in rx_buffer:
        return
    footer_index = rx_buffer.find(FOOT)
    serial_data = rx_buffer[len(HEAD):footer_index]
    decoded = serial_data.decode('utf-8')
    message = json.loads(decoded)
    logger.info("serial read", message=message)
    rx_buffer = rx_buffer[footer_index+len(FOOT):]
    for beacon in message['beacons']:
        if beacon['ID'] == 'Haylou GT1 XR':
            data_lst.append(int(beacon['RSSI']))

ser_ready = False
ser = None
ports = [tuple(p) for p in list(port_list.comports())]
if ports:
    port_name = ports[0][0]
    try:
        if "ttyUSB" in port_name:
            ser = serial.Serial(
                port=port_name,
                baudrate=115200,
                timeout=None
            )
            logger
            ser_ready = True
            logger.info("serial connected", port=port_name)
    except:
        logger.error("Can not connect serial")

serial_recieve_thread = threading.Thread(
    target=serial_recieve,
    daemon=True
)
serial_recieve_thread.start()

ani = animation.FuncAnimation(
    fig, animate, frames=100, fargs=(data_lst,), interval=10
)
plt.show()