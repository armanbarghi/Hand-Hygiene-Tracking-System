import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import json
import time

mqtt_server = '192.168.247.77'
mqtt_port = 1883

def animate(i, data_lst):
    ax.clear()
    ax.plot(data_lst)
    data_lst = data_lst[-100:]
    ax.set_ylim([-100,0])

# callback functions
def on_publish(client, userdata, mid):
    pass

def on_message(client, userdata, msg):
    # Message is an object and the payload property contains the message data which is binary data.
    # The actual message payload is a binary buffer. 
    # In order to decode this payload you need to know what type of data was sent.
    decoded = msg.payload.decode('utf-8')
    message = json.loads(decoded)
    print(message)
    for beacon in message['beacons']:
        if beacon['ID'] == 'Haylou GT1 XR':
            data_lst.append(int(beacon['RSSI']))
    # print('message topic=', msg.topic)
    # print('message qos=', msg.qos)

data_lst = []
fig, ax = plt.subplots()

# Give a name to this MQTT client
client = mqtt.Client('Gateway')
client.on_publish = on_publish
client.message_callback_add('esp32/scan', on_message)

# IP address of your MQTT broker, using ipconfig to look up it
client.connect(mqtt_server, mqtt_port)
# subscribe all topic under esp32/
client.subscribe('esp32/#')
# starts a new thread
client.loop_start()

ani = animation.FuncAnimation(
    fig, animate, frames=100, fargs=(data_lst,), interval=10
)
plt.show()

# status = 0
# while True:
#     msg = "on" if status else "off"
#     status = ~status
#     info = client.publish(
#         topic='esp32/led',
#         payload=msg.encode('utf-8'),
#         qos=0,
#     )
#     # Because published() is not synchronous,
#     # it returns false while he is not aware of delivery that's why calling wait_for_publish() is mandatory.
#     # info.wait_for_publish()
#     time.sleep(1)