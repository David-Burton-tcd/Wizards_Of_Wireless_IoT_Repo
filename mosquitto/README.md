# Mosquitto MQTT broker


1. Start broker.
```
mosquitto.exe -c .\mosquitto.conf -v
```

2. Testing - Subsriber
```
mosquitto_sub -h 172.20.10.10 -t test -u "tclient" -P "mqtttest"
```

3. Testing - Publisher
```
mosquitto_pub -h 172.20.10.10 -t test -m "hello world" -u "tclient" -P "mqtttest"
```