import json
import paho.mqtt.client as mqtt
import ssl

import streamlit as st
import pandas as pd
import plotly.express as px
from datetime import datetime
import numpy as np
import os
import time
import pydeck as pdk

from commons import *


def load_data():
    mod_time = datetime.fromtimestamp(os.path.getmtime(SENSOR_FILE))
    if 'df_last_modified' not in st.session_state or mod_time != st.session_state.df_last_modified:
        st.session_state.df = pd.read_parquet(SENSOR_FILE)
        st.session_state.df_last_modified = mod_time
    return st.session_state.df


def plot_line_chart(df, x, y, title):
    fig = px.line(df, x=x, y=y, title=title)
    fig.update_layout(autosize=False, width=400, height=300)  # Set fixed size for plots
    return fig


def display_device_data(device):
    # Display device information table
    device_info = pd.DataFrame({
        "Device": [device],
        "Label": [SENSOR_LOCATIONS[device]['label']],
        "Version": [SENSOR_VERSIONS[device]]
    })

    # Always use the placeholder to display the updated table
    st.session_state.device_info_placeholder.table(device_info)
    
    device_df = st.session_state.df[st.session_state.df['device'] == device].copy().tail(60)

    last_element_version = device_df["version"].iloc[-1]
    SENSOR_VERSIONS[device] = last_element_version

    # Convert columns to numeric as necessary
    for column in ['avg_speed', 'max_speed', 'min_speed', 'num_cars']:
        device_df[column] = pd.to_numeric(device_df[column], errors='coerce')

    # Update plots
    st.session_state.avg_speed_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'avg_speed', 'Avg Speed Over Time'), use_container_width=True)
    st.session_state.max_speed_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'max_speed', 'Max Speed Over Time'), use_container_width=True)
    st.session_state.min_speed_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'min_speed', 'Min Speed Over Time'), use_container_width=True)
    st.session_state.num_cars_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'num_cars', 'Cars Over Time'), use_container_width=True)


def deploy_action(mqtt_client):
    print("Deploy: MQTT topic: /device/deploy")
    mqtt_client.publish("/device/bump", "deploy")


def retract_action(mqtt_client):
    print("Retract: MQTT topic: /device/deploy")
    mqtt_client.publish("/device/bump", "retract")


def upgrade_action(mqtt_client):
    print("Upgrade: MQTT topic: /device/upgrade")
    mqtt_client.publish("/device/upgrade", "speed_sensor.bin")


def on_mqtt_connect(client, userdata, flags, rc, properties):
    print(f"MQTT client connected with result code {rc}")


def setup_mqtt_client():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    ssl_context = ssl.create_default_context()
    ssl_context.load_verify_locations("cert.pem")
    client.tls_set_context(ssl_context)

    client.on_connect = on_mqtt_connect
    
    client.username_pw_set(*USER_CREDS)
    client.connect(SERVER_HOST, SERVER_PORT, 60)
    client.loop_start()

    return client


def create_dashboard(mqtt_client):
    st.set_page_config(layout="wide")
    st.title("Speed Bump Controller Dashboard")

    load_data()

    devices = np.sort(st.session_state.df['device'].unique())
    selected_device = st.sidebar.selectbox("Select a device", devices)
    
    if 'avg_speed_plot' not in st.session_state:
        prepare_sidebar_placeholders()

    display_device_data(selected_device)

    # Main body map
    view_state = pdk.ViewState(latitude=53.349805, longitude=-6.260310, zoom=12, bearing=0, pitch=0)

    # Create the tooltip for displaying sensor names on hover
    tooltip = {
        "html": "{id}<br>{label}<br>{position}",
        "style": {
            "backgroundColor": "steelblue",
            "color": "white"
        }
    }

    layer = pdk.Layer(
        type="ScatterplotLayer",
        data=[{"label": data["label"], "position": f"({data['lon']}, {data['lat']})", "id": id_} for id_, data in SENSOR_LOCATIONS.items()],
        get_position="position",
        get_color="[180, 0, 200, 140]",
        get_radius=100,
        pickable=True,  # Make layer pickable for the tooltip
    )

    st.pydeck_chart(pdk.Deck(layers=[layer], initial_view_state=view_state, tooltip=tooltip))

    _, _, col3, _, _ = st.columns([1, 1, 2, 1, 1])

    with col3:
        button1, button2, button3 = st.columns([1, 1, 1], gap="small")

        with button1:
            if st.button("Upgrade"):
                upgrade_action(mqtt_client)

        with button2:
            if st.button("Deploy"):
                deploy_action(mqtt_client)
        
        with button3:
            if st.button("Retract"):
                retract_action(mqtt_client)



    while True:
        load_data()
        display_device_data(selected_device)
        time.sleep(UPDATE_INTERVAL)

def prepare_sidebar_placeholders():
    st.session_state.device_info_placeholder = st.sidebar.empty()
    st.session_state.avg_speed_plot = st.sidebar.empty()
    st.session_state.max_speed_plot = st.sidebar.empty()
    st.session_state.min_speed_plot = st.sidebar.empty()
    st.session_state.num_cars_plot = st.sidebar.empty()


def create_login():
    st.title("Login")
    with st.form(key='login_form'):
        username = st.text_input("Username")
        password = st.text_input("Password", type="password")
        submit_button = st.form_submit_button("Login")

        if submit_button:
            if username == "admin" and password == "WowI@t":
                st.session_state.logged_in = True
            else:
                st.error("Invalid username or password")


if __name__ == '__main__':
    mqtt_client = setup_mqtt_client()
    if 'logged_in' not in st.session_state:
        create_login()
    elif st.session_state.logged_in:
        create_dashboard(mqtt_client)
