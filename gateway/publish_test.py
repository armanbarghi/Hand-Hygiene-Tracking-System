import time
import paho.mqtt.client as mqtt

def on_publish(client, userdata, mid):
    print("sent a message")

mqttClient = mqtt.Client("greenhouse_alarm")
mqttClient.on_publish = on_publish
mqttClient.connect('192.168.247.77', 1883)
# start a new thread
mqttClient.loop_start()

# Why use msg.encode('utf-8') here
# MQTT is a binary based protocol where the control elements are binary bytes and not text strings.
# Topic names, Client ID, Usernames and Passwords are encoded as stream of bytes using UTF-8.
status = 0
while True:
    msg = "on" if status else "off"
    status = ~status
    print('msg:', msg)
    info = mqttClient.publish(
        topic='esp32/led',
        payload=msg.encode('utf-8'),
        qos=0,
    )
    # Because published() is not synchronous,
    # it returns false while he is not aware of delivery that's why calling wait_for_publish() is mandatory.
    info.wait_for_publish()
    print('is published:', info.is_published())
    time.sleep(1)