import os
import paho.mqtt.client as mqtt
import pandas as pd
import json
from datetime import datetime

from commons import *

DF = None

def on_mqtt_connect(client, userdata, flags, rc, properties):
    print("Connected with MQTT broker with status", str(rc))

    client.subscribe("test")


def on_mqtt_message(client, userdata, msg):
    global DF

    sensor_readings = json.loads(msg.payload.decode())
    data = sensor_readings.get("data")

    timestamp = datetime.now().isoformat()
    new_row = {'timestamp': timestamp, **data}
    DF = DF._append(new_row, ignore_index=True)
    DF.to_parquet(SENSOR_FILE, index=False)


def main():
    global DF

    if os.path.exists(SENSOR_FILE):
        DF = pd.read_parquet(SENSOR_FILE)
        print("Loaded existing sensor data file.")
    else:
        DF = pd.DataFrame(columns=["timestamp", "speed", "length"])
        print("Initialized new DataFrame.")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_mqtt_connect
    client.on_message = on_mqtt_message

    client.username_pw_set(*USER_CREDS)

    client.connect(SERVER_HOST, SERVER_PORT, 60)
    client.loop_forever()


if __name__ == "__main__":
    main()