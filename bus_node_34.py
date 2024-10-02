import time
import paho.mqtt.client as mqtt
import ssl
import sys
import json
import random
import math
from datetime import datetime

# Parameters
CLOUD_MQTT_URL = "a1kyo3y9afmd0r-ats.iot.eu-west-1.amazonaws.com"
CERTIFICATE_AUTH_FILE = "/home/user/IoT_Aulas_Praticas/PROJECT/AWS certificates/AmazonRootCA1.pem"
CERT_PEM_FILE = "/home/user/IoT_Aulas_Praticas/PROJECT/AWS certificates/2a6d0510986f71aae411ba425cda4a0677ac71d64c12e3195b87ddbf14db966d-certificate.pem.crt"
PRIVATE_KEY_FILE = "/home/user/IoT_Aulas_Praticas/PROJECT/AWS certificates/2a6d0510986f71aae411ba425cda4a0677ac71d64c12e3195b87ddbf14db966d-private.pem.key"
MQTT_TOPIC_PUB = "busses/loc_occ"
MQTT_TOPIC_SUB = "bus/34/stop_rqsts"


# MQTT client setup
client = mqtt.Client()

print("Connecting to Cloud MQTT Broker")
client.tls_set(ca_certs=CERTIFICATE_AUTH_FILE, certfile=CERT_PEM_FILE, keyfile=PRIVATE_KEY_FILE, tls_version=ssl.PROTOCOL_TLSv1_2)
client.tls_insecure_set(False)
client.connect(CLOUD_MQTT_URL, 8883, 60)

# Function to simulate bus position
def get_bus_position(elapsed_time):
    radius = 1.0  # km
    velocity = 50.0  # km/h
    omega = velocity / radius  # angular velocity in rad/h
    omega_rad_sec = omega / 3600.0  # angular velocity in rad/s

    angle = omega_rad_sec * elapsed_time
    latitude = radius * math.sin(angle)
    longitude = radius * math.cos(angle)
    
    return latitude, longitude

# Callback function for received messages
def on_message(client, userdata, message):
    global stop
    payload_json = json.loads(message.payload)
    print(f"Received message '{message.payload.decode()}' on topic '{message.topic}' ")
    print("Stop request payload: ", payload_json["bus_34"])
    if(payload_json["bus_34"] == True):
        stop = True
        print("stop = ", stop)
    


client.on_message = on_message
client.subscribe(MQTT_TOPIC_SUB), 
client.loop_start()

start_time = time.time()
bus_id = "34"
occupancy = random.randint(0, 40)
stop = False
stop_time = 0

try:
    flag = 0
    while True:
        current_time = time.time()
        elapsed_time = current_time - start_time - stop_time
        latitude, longitude = get_bus_position(elapsed_time)
        #print("Distance: ",abs(longitude)-1, "; ", abs(latitude))

        if(abs(longitude-1) < 0.1 and abs(latitude) < 0.1): #arrived at the bus stop
            print("Arrived at bus stop!")
            print("stop =", stop)
            if(stop == True):
                print("Stopped at bus stop!")
                difference_p = occupancy
                stop = False #reset stop flag
                occupancy = random.randint(0, 40) #random people get in and out
                difference_p -= occupancy
                if(difference_p>0):
                    print("This many people exited the bus: ", difference_p)
                else:
                    print("This many people entered the bus: ", abs(difference_p))
                stop_time += 5 
                time.sleep(5)  #Wait 5 seconds before departing
            else:
                print("No stop was requested, so no stop required.")

            
        
        timestamp = int(current_time)

        message = {
            "bus_id": bus_id,
            "ts": str(timestamp),
            "latitude": f"{latitude:.6f}",
            "longitude": f"{longitude:.6f}",
            "occupancy": str(occupancy)
        }

        msg = json.dumps(message)
        
        if(flag>=3):
            print(f"Publishing: {msg}")
            client.publish(MQTT_TOPIC_PUB, msg)
            flag = 0
        
        #else:
            #print("Bus 34 position: ", latitude, "; ", longitude)
            #print("Elapsed time: ", int(elapsed_time))

        flag += 1
        time.sleep(10)  # Publish every minute

except KeyboardInterrupt:
    print("Exiting...")
    client.loop_stop()
    client.disconnect()
    sys.exit()
