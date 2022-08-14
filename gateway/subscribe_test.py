import paho.mqtt.client as mqtt

# a callback function
def on_message(client, userdata, msg):
    # Message is an object and the payload property contains the message data which is binary data.
    # The actual message payload is a binary buffer. 
    # In order to decode this payload you need to know what type of data was sent.
    print('Received a new data ', str(msg.payload.decode('utf-8')))
    print('message topic=', msg.topic)
    print('message qos=', msg.qos)

# Give a name to this MQTT client
client = mqtt.Client('Gateway')
client.message_callback_add('/beacons/office', on_message)

# IP address of your MQTT broker, using ipconfig to look up it  
client.connect('192.168.247.77', 1883)
# 'greenhouse/#' means subscribe all topic under greenhouse
client.subscribe('/beacons/#')

client.loop_forever()
# stop the loop
# client.loop_stop()

# client.loop_start() # starts a new thread