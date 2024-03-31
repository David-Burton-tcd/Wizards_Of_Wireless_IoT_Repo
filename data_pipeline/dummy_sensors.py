import paho.mqtt.client as mqtt
import ssl
import time
import json
import random

from threading import Thread
from commons import *


def on_mqtt_connect(client, userdata, flags, rc, properties):
    print(f"Connected with result code {rc}")


def publish_sensor_data(i, client):
    while True:
        # Generating random sensor data
        sensor_data = {
            "device": f"sensor_{i}",
            "version": "1.0.1",
            "data": {
                "avg_speed": str(round(random.uniform(15, 100), 2)),
                "max_speed": str(round(random.uniform(120, 150), 2)),
                "min_speed": str(round(random.uniform(0, 15), 2)),
                "num_cars": str(random.randint(0, 10)),
                "sensor_1_up": "1",
                "sensor_2_up": "1"
            }
        }
        
        # Publishing the sensor data
        client.publish("/device/data", json.dumps(sensor_data))
        print(f"{i} Published: {sensor_data}")
        
        # Wait for 5 seconds before next publish
        time.sleep(5)


def setup_client():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    ssl_context = ssl.create_default_context()
    ssl_context.load_verify_locations("cert.pem")
    client.tls_set_context(ssl_context)

    client.on_connect = on_mqtt_connect
    
    client.username_pw_set(*USER_CREDS)
    client.connect(SERVER_HOST, SERVER_PORT, 60)
    client.loop_start()

    return client


def main():
    client_threads = [Thread(target=publish_sensor_data, daemon=True, args=(i, setup_client())) for i in range(2, DUMMY_CLIENTS + 2)]
    
    for client_td in client_threads:
        client_td.start()

    for client_td in client_threads:
        client_td.join()


if __name__ == "__main__":
    main()