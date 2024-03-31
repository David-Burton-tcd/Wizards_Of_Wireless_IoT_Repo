SERVER_HOST = "mqtt.wow-iot.ie"
SERVER_PORT = 8883
USER_CREDS = ("data_ingestion", "mqtttest")
SENSOR_FILE = "sensor_readings.parquet"
UPDATE_INTERVAL = 5  # seconds
DUMMY_CLIENTS = 8

SENSOR_VERSIONS = {
    "sensor_0": "",
    "sensor_1": "",
    "sensor_2": "",
    "sensor_3": "",
    "sensor_4": "",
    "sensor_5": "",
    "sensor_6": "",
    "sensor_7": "",
    "sensor_8": "",
    "sensor_9": "",
}

SENSOR_LOCATIONS = {
    "sensor_0": {"lat": 53.353805, "lon": -6.260310, "label": "North of River Liffey"},
    "sensor_1": {"lat": 53.334103, "lon": -6.267493, "label": "Near Dublin Port"},
    "sensor_2": {"lat": 53.338167, "lon": -6.249929, "label": "East towards the coast"},
    "sensor_3": {"lat": 53.353267, "lon": -6.282929, "label": "West near Phoenix Park"},
    "sensor_4": {"lat": 53.348167, "lon": -6.235929, "label": "Further east, coastal"},
    "sensor_5": {"lat": 53.346267, "lon": -6.289929, "label": "Further west"},
    "sensor_6": {"lat": 53.342167, "lon": -6.246929, "label": "East, near Dublin Port"},
    "sensor_7": {"lat": 53.359267, "lon": -6.264929, "label": "North, closer to Drumcondra"},
    "sensor_8": {"lat": 53.345167, "lon": -6.298929, "label": "West, towards Chapelizod"},
    "sensor_9": {"lat": 53.357267, "lon": -6.253929, "label": "North-east, near Clontarf"}
}