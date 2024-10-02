import paho.mqtt.client as mqtt
import ssl
import sys
import json
import time

############ Please change these parameters for your own setup #########

next_bus = "24T"
stop_34 =False
stop_24T = False

# Local MQTT broker URL
LOCAL_MQTT_URL = "localhost"

# Local topics to subscribe to
LOCAL_TOPICS = [
    "status/stop_1"
]

# Cloud MQTT broker details
CLOUD_MQTT_URL = "a1kyo3y9afmd0r-ats.iot.eu-west-1.amazonaws.com"
CERTIFICATE_AUTH_FILE = "/home/user/IoT_Aulas_Praticas/PROJECT/AWS certificates/AmazonRootCA1.pem"
CERT_PEM_FILE = "/home/user/IoT_Aulas_Praticas/PROJECT/AWS certificates/2a6d0510986f71aae411ba425cda4a0677ac71d64c12e3195b87ddbf14db966d-certificate.pem.crt"
PRIVATE_KEY_FILE = "/home/user/IoT_Aulas_Praticas/PROJECT/AWS certificates/2a6d0510986f71aae411ba425cda4a0677ac71d64c12e3195b87ddbf14db966d-private.pem.key"

# Cloud topics to subscribe to
CLOUD_TOPICS = [
    "timings/stop_1"
]

#########################################################################

# Callback for initial local network connection
def on_connect(local_client, userdata, flags, rc):
    print("Connected to local MQTT broker with result code " + str(rc))
    for topic in LOCAL_TOPICS:
        local_client.subscribe(topic)
        print("Subscribed to local topic: " + topic)

# Callback for received message in the local network
def on_local_message(local_client, userdata, msg):
    try:
        # Parse the JSON message
        payload = json.loads(msg.payload)
        #print("payload[Stop request]")
        #print(payload["Stop request"])

        if(payload["Stop request"] == "1"):
            print("Stop requested!")
            if(next_bus == "24T"):
                stop_24T =True
                stop_34 = False
            else:
                stop_24T = False
                stop_34 =True
                
        else:
            stop_24T = False
            stop_34 = False

        current_time = time.time()
        timestamp = int(current_time)

        # Create the first JSON message for "stop_requests"
        stop_request_message = {
            "stop_id": payload["stop_id"],
            "ts": str(timestamp),
            "bus_24T": stop_24T,
            "bus_34" : stop_34
        }

        # Create the second JSON message for "weather"
        weather_message = {
            "stop_id": payload["stop_id"],
            "ts": str(timestamp),
            "Temperature": payload["Temperature"],
            "Humidity": payload["Humidity"],
            "Pollution": payload["Pollution"],
            "Light": payload["Light"]
        }

        # Convert dictionaries to JSON strings
        stop_request_json = json.dumps(stop_request_message)
        weather_json = json.dumps(weather_message)

        # Publish the messages to the cloud
        cloud_client.publish("stop_requests", stop_request_json)
        print(f"Published to cloud topic stop_requests: {stop_request_json}")

        cloud_client.publish("weather", weather_json)
        print(f"Published to cloud topic weather: {weather_json}")

    except Exception as e:
        print(f"Failed to process message: {e}")

# Callback for received message in the cloud
def on_cloud_message(cloud_client, userdata, msg):
    global next_bus
    bus_34_eta = None
    bus_24T_eta = None
    
    # Parse the JSON message
    try:
        message = json.loads(msg.payload.decode('utf-8'))
        
        if not message:
            # print("Empty message received.")
            return
        
        # Initialize variables to store the bus ID with the lowest eta
        lowest_eta = float('inf')
        bus_id_with_lowest_eta = None
        
        # Iterate through the message to find the bus with the lowest eta
        for entry in message:
            bus_id = entry.get('bus_id')
            # print("bus_id: ", bus_id)

            eta = int(entry.get('eta', float('inf')))
            if bus_id == "34":
                # print("Bus 34 eta: ", eta)
                bus_34_eta = eta
            elif bus_id == "24T":
                # print("Bus 24T eta: ", eta)
                bus_24T_eta = eta
            
            if eta < lowest_eta and eta > 0:
                lowest_eta = eta
                bus_id_with_lowest_eta = bus_id
        
        local_timing_message = f"\"bus_34\":\"{bus_34_eta}\",\"bus_24T\":\"{bus_24T_eta}\""

        # Set the global variable next_bus to the bus ID with the lowest eta
        if bus_id_with_lowest_eta is not None:
            next_bus = bus_id_with_lowest_eta
            #print(f"Next bus: {next_bus} with ETA: {lowest_eta}")
        else:
            print("No valid bus_id found in the message.")
        
    except json.JSONDecodeError:
        print("Failed to decode JSON message.")
    
    # Publish the exact same message on the local MQTT broker
    print("Cloud -> Local: Topic [" + msg.topic + "]. Msg \"" + str(msg.payload) + "\"")
    local_client.publish(msg.topic, local_timing_message)
    print(local_timing_message)
    #mosquitto_pub -h localhost -t timings/stop_1 -m "\"bus_34\":\"0\",\"bus_24T\":\"5\""

#########################################################################

# Connect to the Cloud MQTT Client
cloud_client = mqtt.Client()
cloud_client.on_message = on_cloud_message

print("Connecting to the Cloud at " + CLOUD_MQTT_URL + "...")
cloud_client.tls_set(CERTIFICATE_AUTH_FILE, CERT_PEM_FILE, PRIVATE_KEY_FILE, tls_version=ssl.PROTOCOL_TLSv1_2)
cloud_client.tls_insecure_set(False)
cloud_client.connect(CLOUD_MQTT_URL, 8883, 60)
print("Connected to the Cloud MQTT Broker.")

for topic in CLOUD_TOPICS:
    cloud_client.subscribe(topic)
    print("Subscribed to cloud topic: " + topic)

# Connect to the Local MQTT Client
local_client = mqtt.Client()
local_client.on_connect = on_connect
local_client.on_message = on_local_message

print("Connecting locally to " + LOCAL_MQTT_URL + "...")
local_client.connect(LOCAL_MQTT_URL, 1883, 60)

# Main loop
try:
    while True:
        local_client.loop(0.01)  # timeout of 0.01 secs (max 100Hz)
        cloud_client.loop(0.01)

except KeyboardInterrupt:  # catch keyboard interrupts
    sys.exit()

# You can test by running on your VM:
# mosquitto_pub -h localhost -t status/stop_1 -m '{"stop_id ":"stop_1","ts":223,"Stop request":"0","Temperature":"24T.02","Humidity":"67.04","Pollution":"2","Light":"10"}'
# mosquitto_sub -h localhost -t stop_requests
# mosquitto_sub -h localhost -t weather
