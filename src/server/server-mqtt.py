import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import json
import time
import sys
import threading
import structlog


logger = structlog.get_logger(__name__)

mqtt_server = "127.0.0.1"
mqtt_port = 1883

def animate(i, data_lst):
    ax.clear()
    plt.grid()
    ax.plot(data_lst)
    data_lst = data_lst[-100:]
    ax.set_ylim([-100,0])

# region callbacks
def on_publish(client, userdata, mid):
    pass

def on_message(client, userdata, msg):
    decoded = msg.payload.decode('utf-8')
    message = json.loads(decoded)
    logger.info('decoded data', topic=msg.topic, message=message)
    for beacon in message['beacons']:
        if beacon['ID'] == 'Haylou GT1 XR':
            data_lst.append(int(beacon['RSSI']))

def on_connect(clinet, userdata, flags, rc):
    if rc == 0:
        logger.info('MQTT connected', rc=rc)
        client.connected_flag=True #flag to indicate success
    else:
        logger.error('MQTT can not connect', rc=rc)
        client.bad_connection_flag=True
        sys.exit(1)

def on_disconnect(client, userdata, rc):
    logger.error('MQTT disconnected', rc=rc)
# endregion callbacks

def led_blinking():
    status = 0
    while True:
        msg = "on" if status else "off"
        status = ~status
        info = client.publish(
            topic='esp32/led',
            payload=msg.encode('utf-8'),
            qos=0,
        )
        # Because published() is not synchronous,
        # it returns false while he is not aware of delivery that's why calling wait_for_publish() is mandatory.
        # info.wait_for_publish()
        time.sleep(1)

if __name__ == "__main__":
    data_lst = []
    fig, ax = plt.subplots()

    client = mqtt.Client('Gateway')
    client.on_publish = on_publish
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.message_callback_add('esp32/scan', on_message)

    client.connect(mqtt_server, mqtt_port)
    client.subscribe('esp32/#')
    client.loop_start()

    led_blinking_thread = threading.Thread(
        target=led_blinking,
        daemon=True
    )
    # led_blinking_thread.start()

    ani = animation.FuncAnimation(
        fig, animate, frames=100, fargs=(data_lst,), interval=10
    )
    plt.show()